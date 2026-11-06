// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "components/dbus/xdg/portal_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace platform_util {

namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Return;

constexpr char kConnectionName[] = ":1.42";
constexpr char kOpenUriInterface[] = "org.freedesktop.portal.OpenURI";
constexpr char kRequestInterface[] = "org.freedesktop.portal.Request";

struct LaunchedCommand {
  std::string command;
  base::FilePath working_directory;
  std::string arg;

  bool operator==(const LaunchedCommand&) const = default;
};

struct PortalCall {
  std::string method;
  std::string uri;

  bool operator==(const PortalCall&) const = default;
};

class PlatformUtilLinuxTest : public testing::Test {
 protected:
  void SetUp() override {
    bus_ = base::MakeRefCounted<testing::NiceMock<dbus::MockBus>>(
        dbus::Bus::Options());
    dbus_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            bus_.get(), DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS));
    portal_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            bus_.get(), dbus_xdg::kPortalServiceName,
            dbus::ObjectPath(dbus_xdg::kPortalObjectPath));
    request_proxy_ =
        base::MakeRefCounted<testing::NiceMock<dbus::MockObjectProxy>>(
            bus_.get(), dbus_xdg::kPortalServiceName,
            dbus::ObjectPath("/org/freedesktop/portal/desktop/request/1_42/t"));

    ON_CALL(*bus_, GetConnectionName()).WillByDefault(Return(kConnectionName));
    ON_CALL(*bus_,
            GetObjectProxy(DBUS_SERVICE_DBUS, dbus::ObjectPath(DBUS_PATH_DBUS)))
        .WillByDefault(Return(dbus_proxy_.get()));
    ON_CALL(*bus_, GetObjectProxy(dbus_xdg::kPortalServiceName, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath& path) {
          if (path.value() == dbus_xdg::kPortalObjectPath) {
            return portal_proxy_.get();
          }
          request_path_ = path;
          return request_proxy_.get();
        });
    ON_CALL(*bus_, RemoveObjectProxy(dbus_xdg::kPortalServiceName, _, _))
        .WillByDefault([this](std::string_view, const dbus::ObjectPath&,
                              base::OnceClosure callback) {
          ++removed_request_proxies_;
          if (callback) {
            std::move(callback).Run();
          }
          return true;
        });

    ON_CALL(*dbus_proxy_, CallMethod(_, _, _))
        .WillByDefault([this](dbus::MethodCall* call, int,
                              dbus::ObjectProxy::ResponseCallback callback) {
          ++dbus_check_calls_;
          ASSERT_EQ(call->GetMember(), "NameHasOwner");
          dbus::MessageReader reader(call);
          std::string service;
          ASSERT_TRUE(reader.PopString(&service));
          call->SetSerial(1);
          auto response = dbus::Response::CreateEmpty();
          dbus::MessageWriter writer(response.get());
          writer.AppendBool(service == dbus_xdg::kPortalServiceName);
          std::move(callback).Run(response.get());
        });

    ON_CALL(*request_proxy_,
            ConnectToSignal(kRequestInterface, "Response", _, _))
        .WillByDefault(
            [this](
                std::string_view interface_name, std::string_view signal_name,
                dbus::ObjectProxy::SignalCallback signal_callback,
                dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
              response_signal_callback_ = std::move(signal_callback);
              std::move(on_connected_callback)
                  .Run(std::string(interface_name), std::string(signal_name),
                       true);
            });

    ON_CALL(*portal_proxy_, CallMethodWithErrorResponse(_, _, _))
        .WillByDefault(
            [this](dbus::MethodCall* call, int,
                   dbus::ObjectProxy::ResponseOrErrorCallback callback) {
              EXPECT_EQ(call->GetInterface(), kOpenUriInterface);
              dbus::MessageReader reader(call);
              std::string parent_window;
              ASSERT_TRUE(reader.PopString(&parent_window));
              PortalCall recorded;
              recorded.method = call->GetMember();
              if (call->GetMember() == "OpenURI") {
                ASSERT_TRUE(reader.PopString(&recorded.uri));
              } else if (call->GetMember() == "OpenFile") {
                base::ScopedFD fd;
                ASSERT_TRUE(reader.PopFileDescriptor(&fd));
                EXPECT_TRUE(fd.is_valid());
              }
              portal_calls_.push_back(std::move(recorded));

              call->SetSerial(2);
              auto response = dbus::Response::CreateEmpty();
              dbus::MessageWriter writer(response.get());
              writer.AppendObjectPath(request_path_);
              std::move(callback).Run(response.get(), nullptr);
            });

    SetPortalHooksForTesting(
        bus_,
        base::BindLambdaForTesting([this](const std::string& command,
                                          const base::FilePath& working_dir,
                                          const std::string& arg) {
          launched_commands_.push_back({command, working_dir, arg});
        }));
  }

  void TearDown() override { SetPortalHooksForTesting(nullptr, {}); }

  void EmitPortalResponse(uint32_t response_code) {
    ASSERT_TRUE(response_signal_callback_);
    dbus::Signal signal(kRequestInterface, "Response");
    signal.SetSerial(3);
    dbus::MessageWriter writer(&signal);
    writer.AppendUint32(response_code);
    dbus::MessageWriter dict_writer(nullptr);
    writer.OpenArray("{sv}", &dict_writer);
    writer.CloseContainer(&dict_writer);
    response_signal_callback_.Run(&signal);
  }

  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> dbus_proxy_;
  scoped_refptr<dbus::MockObjectProxy> portal_proxy_;
  scoped_refptr<dbus::MockObjectProxy> request_proxy_;
  dbus::ObjectPath request_path_{
      "/org/freedesktop/portal/desktop/request/1_42/default"};
  dbus::ObjectProxy::SignalCallback response_signal_callback_;

  int dbus_check_calls_ = 0;
  int removed_request_proxies_ = 0;
  std::vector<PortalCall> portal_calls_;
  std::vector<LaunchedCommand> launched_commands_;
};

// On unsandboxed host installs, OpenExternal() bypasses
// org.freedesktop.portal.OpenURI entirely and uses xdg-open / xdg-email, so
// schemes handled only by a custom xdg-open, $BROWSER or mailcap entry work.
TEST_F(PlatformUtilLinuxTest, HostUsesXdgOpenWithoutPortal) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kNone);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("sample.txt");
  ASSERT_TRUE(base::WriteFile(temp_file, "hello"));
  const GURL file_url = net::FilePathToFileURL(temp_file);

  OpenExternal(GURL("custom-app://item/42?mode=test"));
  OpenExternal(GURL("https://example.com"));
  OpenExternal(GURL("mailto:dev@example.com"));
  OpenExternal(file_url);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return launched_commands_.size() == 4u; }));

  EXPECT_EQ(dbus_check_calls_, 0);
  EXPECT_THAT(portal_calls_, IsEmpty());
  EXPECT_THAT(
      launched_commands_,
      ElementsAre(
          LaunchedCommand{"xdg-open", base::FilePath(),
                          "custom-app://item/42?mode=test"},
          LaunchedCommand{"xdg-open", base::FilePath(), "https://example.com/"},
          LaunchedCommand{"xdg-email", base::FilePath(),
                          "mailto:dev@example.com"},
          LaunchedCommand{"xdg-open", base::FilePath(), file_url.spec()}));
}

// Inside Flatpak and Snap sandboxes, OpenExternal() uses the OpenURI portal.
// User cancellation does not fall back to xdg-open; other portal errors do.
TEST_F(PlatformUtilLinuxTest, SandboxedUsesOpenUriPortal) {
  int expected_removed = 0;

  for (auto sandbox_type :
       {base::nix::AppSandbox::kFlatpak, base::nix::AppSandbox::kSnap}) {
    base::nix::ScopedAppSandboxOverrideForTesting sandbox(sandbox_type);
    portal_calls_.clear();
    launched_commands_.clear();

    OpenExternal(GURL("custom-app://item/42"));
    ASSERT_TRUE(
        base::test::RunUntil([&] { return portal_calls_.size() == 1u; }));
    EXPECT_THAT(portal_calls_,
                ElementsAre(PortalCall{"OpenURI", "custom-app://item/42"}));

    // User cancellation (response = 1) does not fall back to xdg-open.
    EmitPortalResponse(1);
    ++expected_removed;
    ASSERT_TRUE(base::test::RunUntil(
        [&] { return removed_request_proxies_ == expected_removed; }));
    EXPECT_THAT(launched_commands_, IsEmpty());

    // Other portal errors (response = 2) fall back to xdg-open.
    OpenExternal(GURL("custom-app://item/99"));
    ASSERT_TRUE(
        base::test::RunUntil([&] { return portal_calls_.size() == 2u; }));
    EmitPortalResponse(2);
    ++expected_removed;
    ASSERT_TRUE(base::test::RunUntil([&] {
      return removed_request_proxies_ == expected_removed &&
             launched_commands_.size() == 1u;
    }));
    EXPECT_THAT(launched_commands_,
                ElementsAre(LaunchedCommand{"xdg-open", base::FilePath(),
                                            "custom-app://item/99"}));
  }
}

// Inside a sandbox, file:// URLs use the portal's OpenFile method.
TEST_F(PlatformUtilLinuxTest, SandboxedUsesOpenFilePortalForFileUrls) {
  base::nix::ScopedAppSandboxOverrideForTesting sandbox(
      base::nix::AppSandbox::kFlatpak);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath temp_file = temp_dir.GetPath().AppendASCII("sample.txt");
  ASSERT_TRUE(base::WriteFile(temp_file, "hello"));

  OpenExternal(net::FilePathToFileURL(temp_file));
  ASSERT_TRUE(base::test::RunUntil([&] { return portal_calls_.size() == 1u; }));

  EXPECT_THAT(portal_calls_, ElementsAre(PortalCall{"OpenFile", ""}));
  EmitPortalResponse(0);
  ASSERT_TRUE(
      base::test::RunUntil([&] { return removed_request_proxies_ == 1; }));
  EXPECT_THAT(launched_commands_, IsEmpty());
}

}  // namespace

}  // namespace platform_util
