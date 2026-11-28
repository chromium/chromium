// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/session_manager_client_linux.h"

#include <dlfcn.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/scoped_environment_variable_override.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/browser/lifetime/xsmp_api_linux.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace {

using xsmp::IceConn;
using xsmp::SmPointer;
using xsmp::SmProp;
using xsmp::SmPropValue;

constexpr int kServerCmdFd = 3;
constexpr int kServerEvtFd = 4;

// Minimal X11R6 ICE / XSMP server-side C ABI definitions for the test server.
struct _IceListenObj;
struct _SmsConn;
using IceListenObj = _IceListenObj*;
using SmsConn = _SmsConn*;

enum IceAcceptStatus {
  IceAcceptSuccess = 0,
  IceAcceptFailure = 1,
};

enum IceConnectStatus {
  IceConnectPending = 0,
};

using IceHostBasedAuthProc = int (*)(char*);
using SmsRegisterClientProc = int (*)(SmsConn, SmPointer, char*);
using SmsInteractRequestProc = void (*)(SmsConn, SmPointer, int);
using SmsInteractDoneProc = void (*)(SmsConn, SmPointer, int);
using SmsSaveYourselfRequestProc =
    void (*)(SmsConn, SmPointer, int, int, int, int, int);
using SmsSaveYourselfPhase2RequestProc = void (*)(SmsConn, SmPointer);
using SmsSaveYourselfDoneProc = void (*)(SmsConn, SmPointer, int);
using SmsCloseConnectionProc = void (*)(SmsConn, SmPointer, int, char**);
using SmsSetPropertiesProc = void (*)(SmsConn, SmPointer, int, SmProp**);
using SmsDeletePropertiesProc = void (*)(SmsConn, SmPointer, int, char**);
using SmsGetPropertiesProc = void (*)(SmsConn, SmPointer);

struct SmsCallbacks {
  struct {
    SmsRegisterClientProc callback;
    SmPointer manager_data;
  } register_client;
  struct {
    SmsInteractRequestProc callback;
    SmPointer manager_data;
  } interact_request;
  struct {
    SmsInteractDoneProc callback;
    SmPointer manager_data;
  } interact_done;
  struct {
    SmsSaveYourselfRequestProc callback;
    SmPointer manager_data;
  } save_yourself_request;
  struct {
    SmsSaveYourselfPhase2RequestProc callback;
    SmPointer manager_data;
  } save_yourself_phase2_request;
  struct {
    SmsSaveYourselfDoneProc callback;
    SmPointer manager_data;
  } save_yourself_done;
  struct {
    SmsCloseConnectionProc callback;
    SmPointer manager_data;
  } close_connection;
  struct {
    SmsSetPropertiesProc callback;
    SmPointer manager_data;
  } set_properties;
  struct {
    SmsDeletePropertiesProc callback;
    SmPointer manager_data;
  } delete_properties;
  struct {
    SmsGetPropertiesProc callback;
    SmPointer manager_data;
  } get_properties;
};

using SmsNewClientProc =
    int (*)(SmsConn, SmPointer, unsigned long*, SmsCallbacks*, char**);

constexpr int SmSaveBoth = 2;
constexpr int SmInteractStyleNone = 0;
constexpr unsigned long SmsRegisterClientProcMask = 1UL << 0;
constexpr unsigned long SmsSaveYourselfDoneProcMask = 1UL << 5;
constexpr unsigned long SmsCloseConnectionProcMask = 1UL << 6;
constexpr unsigned long SmsSetPropertiesProcMask = 1UL << 7;

struct ServerFns {
  IceConn (*ice_accept_connection)(IceListenObj, IceAcceptStatus*) = nullptr;
  int (*ice_close_connection)(IceConn) = nullptr;
  char* (*ice_compose_network_id_list)(int, IceListenObj*) = nullptr;
  int (*ice_connection_number)(IceConn) = nullptr;
  IceConnectStatus (*ice_connection_status)(IceConn) = nullptr;
  void (*ice_free_listen_objs)(int, IceListenObj*) = nullptr;
  int (*ice_get_listen_connection_number)(IceListenObj) = nullptr;
  int (*ice_listen_for_connections)(int*, IceListenObj**, int, char*) = nullptr;
  int (*ice_process_messages)(IceConn, void*, int*) = nullptr;
  void (*ice_set_host_based_auth_proc)(IceListenObj,
                                       IceHostBasedAuthProc) = nullptr;
  void (*ice_set_shutdown_negotiation)(IceConn, int) = nullptr;
  void (*sm_free_property)(SmProp*) = nullptr;
  void (*sm_free_reasons)(int, char**) = nullptr;
  void (*sms_clean_up)(SmsConn) = nullptr;
  void (*sms_die)(SmsConn) = nullptr;
  IceConn (*sms_get_ice_connection)(SmsConn) = nullptr;
  int (*sms_initialize)(const char*,
                        const char*,
                        SmsNewClientProc,
                        SmPointer,
                        IceHostBasedAuthProc,
                        int,
                        char*) = nullptr;
  int (*sms_register_client_reply)(SmsConn, char*) = nullptr;
  void (*sms_save_yourself)(SmsConn, int, int, int, int) = nullptr;
};

struct ServerState {
  ServerFns fns;
  RAW_PTR_EXCLUSION SmsConn sms_conn = nullptr;
  bool closed = false;
};

void WriteAll(int fd, std::string_view data) {
  base::WriteFileDescriptor(fd, data);
}

int AllowHostBasedAuth(char* /*hostname*/) {
  return 1;
}

DISABLE_CFI_DLSYM int OnRegisterClient(SmsConn sms_conn,
                                       SmPointer manager_data,
                                       char* previous_id) {
  auto* state = static_cast<ServerState*>(manager_data);
  std::string prev = previous_id ? previous_id : "<none>";
  free(previous_id);
  WriteAll(kServerEvtFd, "REGISTERED " + prev + "\n");
  return state->fns.sms_register_client_reply(
      sms_conn, const_cast<char*>("test-client-id-42"));
}

DISABLE_CFI_DLSYM void OnSetProperties(SmsConn /*sms_conn*/,
                                       SmPointer manager_data,
                                       int num_props,
                                       SmProp** props) {
  auto* state = static_cast<ServerState*>(manager_data);
  std::string program;
  int style = -1;
  std::vector<std::string> restart_cmd;
  std::vector<std::string> clone_cmd;

  // SAFETY: `props` has `num_props` elements allocated by libSM.
  auto prop_span =
      UNSAFE_BUFFERS(base::span(props, static_cast<size_t>(num_props)));
  for (SmProp* prop : prop_span) {
    std::string_view name(prop->name);
    // SAFETY: `prop->vals` has `prop->num_vals` elements allocated by libSM.
    auto vals = UNSAFE_BUFFERS(
        base::span(prop->vals, static_cast<size_t>(prop->num_vals)));
    if (name == xsmp::SmProgram && !vals.empty()) {
      program.assign(static_cast<const char*>(vals[0].value), vals[0].length);
    } else if (name == xsmp::SmRestartStyleHint && !vals.empty()) {
      style = *static_cast<const unsigned char*>(vals[0].value);
    } else if (name == xsmp::SmRestartCommand) {
      for (const SmPropValue& v : vals) {
        restart_cmd.emplace_back(static_cast<const char*>(v.value), v.length);
      }
    } else if (name == xsmp::SmCloneCommand) {
      for (const SmPropValue& v : vals) {
        clone_cmd.emplace_back(static_cast<const char*>(v.value), v.length);
      }
    }
    state->fns.sm_free_property(prop);
  }
  free(props);

  WriteAll(kServerEvtFd, "PROPS " + program + "|" +
                             base::NumberToString(style) + "|" +
                             base::JoinString(restart_cmd, ",") + "|" +
                             base::JoinString(clone_cmd, ",") + "\n");
}

DISABLE_CFI_DLSYM void OnSaveYourselfDone(SmsConn /*sms_conn*/,
                                          SmPointer /*manager_data*/,
                                          int success) {
  WriteAll(kServerEvtFd, "SAVED " + base::NumberToString(success) + "\n");
}

DISABLE_CFI_DLSYM void OnCloseConnection(SmsConn sms_conn,
                                         SmPointer manager_data,
                                         int count,
                                         char** reason_msgs) {
  auto* state = static_cast<ServerState*>(manager_data);
  // libSM allocates `reason_msgs` with `malloc(0)` even when `count == 0`.
  state->fns.sm_free_reasons(count, reason_msgs);
  IceConn ice_conn = state->fns.sms_get_ice_connection(sms_conn);
  state->fns.sms_clean_up(sms_conn);
  state->fns.ice_set_shutdown_negotiation(ice_conn, 0);
  state->fns.ice_close_connection(ice_conn);
  state->sms_conn = nullptr;
  state->closed = true;
  WriteAll(kServerEvtFd, "CLOSED\n");
}

int OnNewClient(SmsConn sms_conn,
                SmPointer manager_data,
                unsigned long* mask_ret,
                SmsCallbacks* callbacks_ret,
                char** /*failure_reason_ret*/) {
  auto* state = static_cast<ServerState*>(manager_data);
  state->sms_conn = sms_conn;
  *mask_ret = SmsRegisterClientProcMask | SmsSetPropertiesProcMask |
              SmsSaveYourselfDoneProcMask | SmsCloseConnectionProcMask;
  callbacks_ret->register_client.callback = &OnRegisterClient;
  callbacks_ret->register_client.manager_data = state;
  callbacks_ret->set_properties.callback = &OnSetProperties;
  callbacks_ret->set_properties.manager_data = state;
  callbacks_ret->save_yourself_done.callback = &OnSaveYourselfDone;
  callbacks_ret->save_yourself_done.manager_data = state;
  callbacks_ret->close_connection.callback = &OnCloseConnection;
  callbacks_ret->close_connection.manager_data = state;
  return 1;
}

class EventPipeReader {
 public:
  explicit EventPipeReader(int fd) : fd_(fd) {}

  std::string ReadLine() {
    while (true) {
      size_t newline = buffer_.find('\n');
      if (newline != std::string::npos) {
        std::string line = buffer_.substr(0, newline);
        buffer_.erase(0, newline + 1);
        return line;
      }

      base::RunLoop run_loop;
      auto watcher = base::FileDescriptorWatcher::WatchReadable(
          fd_, base::BindRepeating(&EventPipeReader::OnReadable,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   run_loop.QuitClosure()));
      run_loop.Run();
    }
  }

 private:
  void OnReadable(base::RepeatingClosure quit_closure) {
    std::array<char, 512> tmp;
    ssize_t n = read(fd_, tmp.data(), tmp.size());
    if (n > 0) {
      buffer_.append(tmp.data(), static_cast<size_t>(n));
    }
    quit_closure.Run();
  }

  int fd_;
  std::string buffer_;
  base::WeakPtrFactory<EventPipeReader> weak_ptr_factory_{this};
};

DISABLE_CFI_DLSYM int RunXsmpTestServer() {
  void* ice_lib = dlopen("libICE.so.6", RTLD_LAZY | RTLD_LOCAL);
  void* sm_lib = dlopen("libSM.so.6", RTLD_LAZY | RTLD_LOCAL);
  if (!ice_lib || !sm_lib) {
    WriteAll(kServerEvtFd, "UNAVAILABLE\n");
    return 0;
  }

  ServerState state;
  auto load = [](void* lib, const char* name, auto& fn) {
    fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(
        dlsym(lib, name));
    return fn != nullptr;
  };
  if (!load(ice_lib, "IceAcceptConnection", state.fns.ice_accept_connection) ||
      !load(ice_lib, "IceCloseConnection", state.fns.ice_close_connection) ||
      !load(ice_lib, "IceComposeNetworkIdList",
            state.fns.ice_compose_network_id_list) ||
      !load(ice_lib, "IceConnectionNumber", state.fns.ice_connection_number) ||
      !load(ice_lib, "IceConnectionStatus", state.fns.ice_connection_status) ||
      !load(ice_lib, "IceFreeListenObjs", state.fns.ice_free_listen_objs) ||
      !load(ice_lib, "IceGetListenConnectionNumber",
            state.fns.ice_get_listen_connection_number) ||
      !load(ice_lib, "IceListenForConnections",
            state.fns.ice_listen_for_connections) ||
      !load(ice_lib, "IceProcessMessages", state.fns.ice_process_messages) ||
      !load(ice_lib, "IceSetHostBasedAuthProc",
            state.fns.ice_set_host_based_auth_proc) ||
      !load(ice_lib, "IceSetShutdownNegotiation",
            state.fns.ice_set_shutdown_negotiation) ||
      !load(sm_lib, "SmFreeProperty", state.fns.sm_free_property) ||
      !load(sm_lib, "SmFreeReasons", state.fns.sm_free_reasons) ||
      !load(sm_lib, "SmsCleanUp", state.fns.sms_clean_up) ||
      !load(sm_lib, "SmsDie", state.fns.sms_die) ||
      !load(sm_lib, "SmsGetIceConnection", state.fns.sms_get_ice_connection) ||
      !load(sm_lib, "SmsInitialize", state.fns.sms_initialize) ||
      !load(sm_lib, "SmsRegisterClientReply",
            state.fns.sms_register_client_reply) ||
      !load(sm_lib, "SmsSaveYourself", state.fns.sms_save_yourself)) {
    return 1;
  }

  std::array<char, 256> err = {};
  if (!state.fns.sms_initialize("ChromiumTestSM", "1.0", &OnNewClient, &state,
                                &AllowHostBasedAuth, err.size(), err.data())) {
    return 1;
  }

  int listen_count = 0;
  IceListenObj* listen_objs = nullptr;
  if (!state.fns.ice_listen_for_connections(&listen_count, &listen_objs,
                                            err.size(), err.data()) ||
      listen_count < 1) {
    return 1;
  }
  // SAFETY: `listen_objs` has `listen_count` elements returned by libICE.
  auto listen_span = UNSAFE_BUFFERS(
      base::span(listen_objs, static_cast<size_t>(listen_count)));
  for (IceListenObj obj : listen_span) {
    state.fns.ice_set_host_based_auth_proc(obj, &AllowHostBasedAuth);
  }

  char* net_ids =
      state.fns.ice_compose_network_id_list(listen_count, listen_objs);
  WriteAll(kServerEvtFd, std::string("LISTEN ") + net_ids + "\n");
  free(net_ids);

  IceConn client_ice = nullptr;
  std::string cmd_buf;
  while (!state.closed) {
    std::vector<struct pollfd> pfds;
    pfds.push_back({.fd = kServerCmdFd, .events = POLLIN});
    for (IceListenObj obj : listen_span) {
      pfds.push_back({.fd = state.fns.ice_get_listen_connection_number(obj),
                      .events = POLLIN});
    }
    if (client_ice) {
      pfds.push_back({.fd = state.fns.ice_connection_number(client_ice),
                      .events = POLLIN});
    }

    if (poll(pfds.data(), pfds.size(), 10000) <= 0) {
      break;
    }

    for (size_t i = 0; i < listen_span.size(); ++i) {
      if (pfds[1 + i].revents & POLLIN) {
        IceAcceptStatus status = IceAcceptFailure;
        client_ice = state.fns.ice_accept_connection(listen_span[i], &status);
        while (client_ice && state.fns.ice_connection_status(client_ice) ==
                                 IceConnectPending) {
          state.fns.ice_process_messages(client_ice, nullptr, nullptr);
        }
      }
    }

    if (client_ice &&
        pfds.back().fd == state.fns.ice_connection_number(client_ice) &&
        (pfds.back().revents & (POLLIN | POLLHUP))) {
      state.fns.ice_process_messages(client_ice, nullptr, nullptr);
      if (state.closed) {
        break;
      }
    }

    if (pfds[0].revents & (POLLIN | POLLHUP)) {
      std::array<char, 128> buf;
      ssize_t n = read(kServerCmdFd, buf.data(), buf.size());
      if (n <= 0) {
        break;
      }
      cmd_buf.append(buf.data(), static_cast<size_t>(n));
      size_t nl;
      while ((nl = cmd_buf.find('\n')) != std::string::npos) {
        std::string cmd = cmd_buf.substr(0, nl);
        cmd_buf.erase(0, nl + 1);
        if (cmd == "SAVE_YOURSELF" && state.sms_conn) {
          state.fns.sms_save_yourself(state.sms_conn, SmSaveBoth,
                                      /*shutdown=*/1, SmInteractStyleNone,
                                      /*fast=*/0);
        } else if (cmd == "DIE" && state.sms_conn) {
          state.fns.sms_die(state.sms_conn);
        }
      }
    }
  }

  state.fns.ice_free_listen_objs(listen_count, listen_objs);
  return 0;
}

}  // namespace

MULTIPROCESS_TEST_MAIN(XsmpTestServer) {
  return RunXsmpTestServer();
}

TEST(SessionManagerClientLinuxTest, RegistersAndHandlesSessionLifecycle) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedEnvironmentVariableOverride ice_auth_env(
      "ICEAUTHORITY", temp_dir.GetPath().Append("ICEauthority").value());

  std::array<int, 2> cmd_pipe = {-1, -1};
  std::array<int, 2> evt_pipe = {-1, -1};
  ASSERT_EQ(0, pipe(cmd_pipe.data()));
  ASSERT_EQ(0, pipe(evt_pipe.data()));
  base::ScopedFD cmd_read(cmd_pipe[0]);
  base::ScopedFD cmd_write(cmd_pipe[1]);
  base::ScopedFD evt_read(evt_pipe[0]);
  base::ScopedFD evt_write(evt_pipe[1]);

  base::LaunchOptions options;
  options.fds_to_remap.emplace_back(cmd_read.get(), kServerCmdFd);
  options.fds_to_remap.emplace_back(evt_write.get(), kServerEvtFd);
  base::Process server = base::SpawnMultiProcessTestChild(
      "XsmpTestServer", base::GetMultiProcessTestChildBaseCommandLine(),
      options);
  ASSERT_TRUE(server.IsValid());
  cmd_read.reset();
  evt_write.reset();

  EventPipeReader reader(evt_read.get());
  std::string listen_line = reader.ReadLine();
  if (listen_line == "UNAVAILABLE") {
    GTEST_SKIP() << "libSM.so.6 / libICE.so.6 not available";
  }
  ASSERT_TRUE(base::StartsWith(listen_line, "LISTEN ")) << listen_line;
  base::ScopedEnvironmentVariableOverride sm_env(
      "SESSION_MANAGER", listen_line.substr(strlen("LISTEN ")));

  base::CommandLine cmd(base::FilePath("/usr/bin/google-chrome-stable"));
  cmd.AppendSwitchASCII(kSmClientIdSwitch, "prev-id-7");
  cmd.AppendArg("https://example.com");

  auto save_count = base::MakeRefCounted<base::RefCountedData<int>>(0);
  auto die_count = base::MakeRefCounted<base::RefCountedData<int>>(0);
  auto client = SessionManagerClientLinux::Connect(
      cmd,
      base::BindRepeating(
          [](scoped_refptr<base::RefCountedData<int>> count) { ++count->data; },
          save_count),
      base::BindOnce(
          [](scoped_refptr<base::RefCountedData<int>> count) { ++count->data; },
          die_count));
  ASSERT_NE(nullptr, client);
  EXPECT_EQ("test-client-id-42", client->client_id());

  EXPECT_EQ("REGISTERED prev-id-7", reader.ReadLine());
  EXPECT_EQ(
      "PROPS /usr/bin/google-chrome-stable|0|"
      "/usr/bin/google-chrome-stable,--sm-client-id=prev-id-7,"
      "https://example.com|/usr/bin/google-chrome-stable,"
      "--sm-client-id=prev-id-7,https://example.com",
      reader.ReadLine());

  WriteAll(cmd_write.get(), "SAVE_YOURSELF\n");
  EXPECT_EQ("SAVED 1", reader.ReadLine());
  EXPECT_EQ(1, save_count->data);
  EXPECT_EQ(0, die_count->data);

  WriteAll(cmd_write.get(), "DIE\n");
  EXPECT_EQ("CLOSED", reader.ReadLine());
  EXPECT_EQ(1, die_count->data);

  int exit_code = -1;
  EXPECT_TRUE(server.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}
