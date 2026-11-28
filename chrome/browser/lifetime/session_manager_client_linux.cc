// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/session_manager_client_linux.h"

#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/environment.h"
#include "base/files/file_descriptor_watcher_posix.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/lifetime/xsmp_api_linux.h"

using xsmp::IceConn;
using xsmp::SmcConn;
using xsmp::SmPointer;
using xsmp::SmProp;
using xsmp::SmPropValue;
using xsmp::XsmpApi;

namespace {

std::string GetCurrentUserName() {
  std::array<char, 1024> buf = {};
  struct passwd pw = {};
  struct passwd* result = nullptr;
  if (getpwuid_r(geteuid(), &pw, buf.data(), buf.size(), &result) == 0 &&
      result && result->pw_name) {
    return result->pw_name;
  }
  auto env = base::Environment::Create();
  return env->GetVar("USER").value_or("");
}

}  // namespace

struct SessionManagerClientLinux::Callbacks {
  static void SaveYourself(SmcConn /*smc_conn*/,
                           SmPointer client_data,
                           int save_type,
                           int /*shutdown*/,
                           int /*interact_style*/,
                           int /*fast*/) {
    static_cast<SessionManagerClientLinux*>(client_data)
        ->OnSaveYourself(save_type);
  }

  static void Die(SmcConn /*smc_conn*/, SmPointer client_data) {
    static_cast<SessionManagerClientLinux*>(client_data)->die_received_ = true;
  }

  static void SaveComplete(SmcConn /*smc_conn*/, SmPointer /*client_data*/) {}

  static void ShutdownCancelled(SmcConn /*smc_conn*/,
                                SmPointer /*client_data*/) {}
};

// static
DISABLE_CFI_DLSYM std::unique_ptr<SessionManagerClientLinux>
SessionManagerClientLinux::Connect(const base::CommandLine& command_line,
                                   base::RepeatingClosure save_state_cb,
                                   base::OnceClosure die_cb) {
  auto env = base::Environment::Create();
  std::optional<std::string> session_manager = env->GetVar("SESSION_MANAGER");
  if (!session_manager || session_manager->empty()) {
    return nullptr;
  }

  const XsmpApi* api = XsmpApi::Get();
  if (!api) {
    return nullptr;
  }

  auto client = base::WrapUnique(new SessionManagerClientLinux(
      std::move(save_state_cb), std::move(die_cb)));

  xsmp::SmcCallbacks callbacks = {};
  callbacks.save_yourself = {&Callbacks::SaveYourself, client.get()};
  callbacks.die = {&Callbacks::Die, client.get()};
  callbacks.save_complete = {&Callbacks::SaveComplete, client.get()};
  callbacks.shutdown_cancelled = {&Callbacks::ShutdownCancelled, client.get()};
  constexpr unsigned long kCallbackMask =
      xsmp::SmcSaveYourselfProcMask | xsmp::SmcDieProcMask |
      xsmp::SmcSaveCompleteProcMask | xsmp::SmcShutdownCancelledProcMask;

  std::string network_ids = *session_manager;
  std::string previous_id = command_line.GetSwitchValueASCII(kSmClientIdSwitch);
  char* client_id_ret = nullptr;
  std::array<char, 256> error_buf = {};

  SmcConn smc_conn = api->open_connection(
      network_ids.data(), /*context=*/nullptr, xsmp::SmProtoMajor,
      xsmp::SmProtoMinor, kCallbackMask, &callbacks,
      previous_id.empty() ? nullptr : previous_id.c_str(), &client_id_ret,
      error_buf.size(), error_buf.data());
  if (!smc_conn && !previous_id.empty()) {
    // Some session managers reject an unrecognized previous_id; retry as a
    // fresh client.
    smc_conn = api->open_connection(network_ids.data(), /*context=*/nullptr,
                                    xsmp::SmProtoMajor, xsmp::SmProtoMinor,
                                    kCallbackMask, &callbacks,
                                    /*previous_id=*/nullptr, &client_id_ret,
                                    error_buf.size(), error_buf.data());
  }
  if (!smc_conn) {
    VLOG(1) << "Failed to open XSMP connection: " << error_buf.data();
    return nullptr;
  }

  if (client_id_ret) {
    client->client_id_ = client_id_ret;
    free(client_id_ret);
  }
  client->smc_conn_ = smc_conn;

  IceConn ice_conn = api->get_ice_connection(smc_conn);
  int fd = ice_conn ? api->connection_number(ice_conn) : -1;
  if (fd < 0) {
    client->Disconnect(/*close_smc=*/true);
    return nullptr;
  }
  base::SetCloseOnExec(fd);

  client->SetProperties(command_line);
  client->watch_controller_ = base::FileDescriptorWatcher::WatchReadable(
      fd, base::BindRepeating(&SessionManagerClientLinux::OnIceReadable,
                              client->weak_ptr_factory_.GetWeakPtr()));
  return client;
}

SessionManagerClientLinux::SessionManagerClientLinux(
    base::RepeatingClosure save_state_cb,
    base::OnceClosure die_cb)
    : save_state_cb_(std::move(save_state_cb)), die_cb_(std::move(die_cb)) {}

SessionManagerClientLinux::~SessionManagerClientLinux() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Disconnect(/*close_smc=*/true);
}

DISABLE_CFI_DLSYM void SessionManagerClientLinux::SetProperties(
    const base::CommandLine& command_line) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const XsmpApi* api = XsmpApi::Get();
  if (!api || !smc_conn_) {
    return;
  }

  const base::CommandLine& clone_cmd = command_line;
  const base::CommandLine& restart_cmd = command_line;

  std::string program = restart_cmd.GetProgram().value();
  std::string user_id = GetCurrentUserName();
  unsigned char restart_style = xsmp::SmRestartIfRunning;

  auto to_prop_values = [](const std::vector<std::string>& args) {
    std::vector<SmPropValue> values;
    values.reserve(args.size());
    for (const std::string& arg : args) {
      values.push_back(
          {static_cast<int>(arg.size()), const_cast<char*>(arg.data())});
    }
    return values;
  };
  std::vector<SmPropValue> clone_vals = to_prop_values(clone_cmd.argv());
  std::vector<SmPropValue> restart_vals = to_prop_values(restart_cmd.argv());

  auto make_prop = [](const char* name, const char* type,
                      std::vector<SmPropValue>& vals) {
    return SmProp{const_cast<char*>(name), const_cast<char*>(type),
                  static_cast<int>(vals.size()), vals.data()};
  };
  std::vector<SmPropValue> program_vals = {
      {static_cast<int>(program.size()), program.data()}};
  std::vector<SmPropValue> style_vals = {{1, &restart_style}};
  std::vector<SmPropValue> user_vals = {
      {static_cast<int>(user_id.size()), user_id.data()}};
  SmProp program_prop =
      make_prop(xsmp::SmProgram, xsmp::SmARRAY8, program_vals);
  SmProp clone_prop =
      make_prop(xsmp::SmCloneCommand, xsmp::SmLISTofARRAY8, clone_vals);
  SmProp restart_prop =
      make_prop(xsmp::SmRestartCommand, xsmp::SmLISTofARRAY8, restart_vals);
  SmProp style_prop =
      make_prop(xsmp::SmRestartStyleHint, xsmp::SmCARD8, style_vals);
  SmProp user_prop = make_prop(xsmp::SmUserID, xsmp::SmARRAY8, user_vals);

  std::vector<SmProp*> props = {&program_prop, &clone_prop, &restart_prop,
                                &style_prop};
  if (!user_id.empty()) {
    props.push_back(&user_prop);
  }
  api->set_properties(smc_conn_, static_cast<int>(props.size()), props.data());
}

DISABLE_CFI_DLSYM void SessionManagerClientLinux::OnIceReadable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const XsmpApi* api = XsmpApi::Get();
  if (!api || !smc_conn_) {
    return;
  }

  IceConn ice_conn = api->get_ice_connection(smc_conn_);
  const xsmp::IceProcessMessagesStatus status =
      api->process_messages(ice_conn, nullptr, nullptr);
  if (status == xsmp::IceProcessMessagesIOError) {
    Disconnect(/*close_smc=*/true);
    return;
  }
  if (status == xsmp::IceProcessMessagesConnectionClosed) {
    // IceProcessMessages already freed the underlying IceConn.
    Disconnect(/*close_smc=*/false);
    return;
  }

  if (die_received_) {
    Disconnect(/*close_smc=*/true);
    if (die_cb_) {
      std::move(die_cb_).Run();
    }
  }
}

DISABLE_CFI_DLSYM void SessionManagerClientLinux::OnSaveYourself(
    int save_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (save_type != xsmp::SmSaveGlobal && save_state_cb_) {
    save_state_cb_.Run();
  }
  if (const XsmpApi* api = XsmpApi::Get(); api && smc_conn_) {
    api->save_yourself_done(smc_conn_, /*success=*/1);
  }
}

DISABLE_CFI_DLSYM void SessionManagerClientLinux::Disconnect(bool close_smc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Stop watching the ICE socket before closing it so the fd outlives the
  // watcher controller.
  watch_controller_.reset();
  SmcConn conn = smc_conn_;
  smc_conn_ = nullptr;
  if (conn && close_smc) {
    if (const XsmpApi* api = XsmpApi::Get()) {
      api->close_connection(conn, 0, nullptr);
    }
  }
}
