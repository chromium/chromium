// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "chrome/browser/devtools/devtools_settings.h"
#include "chrome/browser/devtools/visual_logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::StrictMock;

using DispatchCallback = DevToolsEmbedderMessageDispatcher::DispatchCallback;

class MockDelegate : public DevToolsEmbedderMessageDispatcher::Delegate {
 public:
  MOCK_METHOD(bool, IsLocalFrontend, (), (const, override));
  MOCK_METHOD(void, ActivateWindow, (), (override));
  MOCK_METHOD(void, CloseWindow, (), (override));
  MOCK_METHOD(void, LoadCompleted, (), (override));
  MOCK_METHOD(void, SetInspectedPageBounds, (const gfx::Rect&), (override));
  MOCK_METHOD(void, InspectElementCompleted, (), (override));
  MOCK_METHOD(void, InspectedURLChanged, (const std::string&), (override));
  MOCK_METHOD(void, SetIsDocked, (DispatchCallback, bool), (override));
  MOCK_METHOD(void,
              OpenInNewTab,
              (const std::string&, const std::optional<std::string>&),
              (override));
  MOCK_METHOD(void,
              OpenSearchResultsInNewTab,
              (const std::string&),
              (override));
  MOCK_METHOD(void, ShowItemInFolder, (const std::string&), (override));
  MOCK_METHOD(void,
              SaveToFile,
              (const std::string&, const std::string&, bool, bool),
              (override));
  MOCK_METHOD(void,
              AppendToFile,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, RequestFileSystems, (), (override));
  MOCK_METHOD(void, AddFileSystem, (const std::string&), (override));
  MOCK_METHOD(void, RemoveFileSystem, (const std::string&), (override));
  MOCK_METHOD(void,
              UpgradeDraggedFileSystemPermissions,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              ConnectAutomaticFileSystem,
              (DispatchCallback, const std::string&, const std::string&, bool),
              (override));
  MOCK_METHOD(void,
              DisconnectAutomaticFileSystem,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              IndexPath,
              (int, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, StopIndexing, (int), (override));
  MOCK_METHOD(void,
              LoadNetworkResource,
              (DispatchCallback, const std::string&, const std::string&, int),
              (override));
  MOCK_METHOD(void,
              SearchInPath,
              (int, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, SetWhitelistedShortcuts, (const std::string&), (override));
  MOCK_METHOD(void, SetEyeDropperActive, (bool), (override));
  MOCK_METHOD(void, ShowCertificateViewer, (const std::string&), (override));
  MOCK_METHOD(void, ZoomIn, (), (override));
  MOCK_METHOD(void, ZoomOut, (), (override));
  MOCK_METHOD(void, ResetZoom, (), (override));
  MOCK_METHOD(void, SetDevicesUpdatesEnabled, (bool), (override));
  MOCK_METHOD(void,
              SetDevicesDiscoveryConfig,
              (bool, bool, const std::string&, bool, const std::string&),
              (override));
  MOCK_METHOD(void,
              OpenRemotePage,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, OpenNodeFrontend, (), (override));
  MOCK_METHOD(void,
              RegisterPreference,
              (const std::string&, const RegisterOptions&),
              (override));
  MOCK_METHOD(void, GetPreferences, (DispatchCallback), (override));
  MOCK_METHOD(void,
              GetPreference,
              (DispatchCallback, const std::string&),
              (override));
  MOCK_METHOD(void,
              SetPreference,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, RemovePreference, (const std::string&), (override));
  MOCK_METHOD(void, ClearPreferences, (), (override));
  MOCK_METHOD(void, GetSyncInformation, (DispatchCallback), (override));
  MOCK_METHOD(void, GetHostConfig, (DispatchCallback), (override));
  MOCK_METHOD(void,
              DispatchProtocolMessageFromDevToolsFrontend,
              (const std::string&),
              (override));
  MOCK_METHOD(void,
              RecordCountHistogram,
              (const std::string&, int, int, int, int),
              (override));
  MOCK_METHOD(void,
              RecordEnumeratedHistogram,
              (const std::string&, int, int),
              (override));
  MOCK_METHOD(void,
              RecordPerformanceHistogram,
              (const std::string&, double),
              (override));
  MOCK_METHOD(void,
              RecordPerformanceHistogramMedium,
              (const std::string&, double),
              (override));
  MOCK_METHOD(void, RecordUserMetricsAction, (const std::string&), (override));
  MOCK_METHOD(void, RecordNewBadgeUsage, (const std::string&), (override));
  MOCK_METHOD(void, RecordImpression, (const ImpressionEvent&), (override));
  MOCK_METHOD(void, RecordResize, (const ResizeEvent&), (override));
  MOCK_METHOD(void, RecordClick, (const ClickEvent&), (override));
  MOCK_METHOD(void, RecordHover, (const HoverEvent&), (override));
  MOCK_METHOD(void, RecordDrag, (const DragEvent&), (override));
  MOCK_METHOD(void, RecordChange, (const ChangeEvent&), (override));
  MOCK_METHOD(void, RecordKeyDown, (const KeyDownEvent&), (override));
  MOCK_METHOD(void,
              RecordSettingAccess,
              (const SettingAccessEvent&),
              (override));
  MOCK_METHOD(void, RecordFunctionCall, (const FunctionCallEvent&), (override));
  MOCK_METHOD(void, Reattach, (DispatchCallback), (override));
  MOCK_METHOD(void, ReadyForTest, (), (override));
  MOCK_METHOD(void, ConnectionReady, (), (override));
  MOCK_METHOD(void, SetOpenNewWindowForPopups, (bool), (override));
  MOCK_METHOD(void,
              RegisterExtensionsAPI,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              ShowSurvey,
              (DispatchCallback, const std::string&),
              (override));
  MOCK_METHOD(void,
              CanShowSurvey,
              (DispatchCallback, const std::string&),
              (override));
  MOCK_METHOD(void,
              DoAidaConversation,
              (DispatchCallback, const std::string&, int),
              (override));
  MOCK_METHOD(void,
              AidaCodeComplete,
              (DispatchCallback, const std::string&),
              (override));
  MOCK_METHOD(void,
              RegisterAidaClientEvent,
              (DispatchCallback, const std::string&),
              (override));
  MOCK_METHOD(void, SetChromeFlag, (const std::string&, bool), (override));
  MOCK_METHOD(void,
              DispatchHttpRequest,
              (DispatchCallback, const DevToolsDispatchHttpRequestParams&),
              (override));
  MOCK_METHOD(void, RequestRestart, (), (override));
};

// Tests how the dispatcher parses the arguments of an embedder message into the
// parameters of the delegate method that handles it. The delegate is a
// StrictMock, so a message that is dispatched to any method without an
// EXPECT_CALL fails the test.
class DevToolsEmbedderMessageDispatcherTest : public testing::Test {
 protected:
  // Dispatches `method` with `params` and returns whether the dispatcher
  // accepted the message, i.e. knew the method and could parse the arguments.
  // `response_` receives the dispatcher's response, if it gave one.
  bool Dispatch(std::string_view method,
                const base::ListValue& params = base::ListValue()) {
    response_.reset();
    return dispatcher_->Dispatch(
        base::BindLambdaForTesting([&](const base::Value* response) {
          response_ = response ? response->Clone() : base::Value();
        }),
        std::string(method), params);
  }

  StrictMock<MockDelegate> delegate_;
  std::unique_ptr<DevToolsEmbedderMessageDispatcher> dispatcher_ =
      DevToolsEmbedderMessageDispatcher::CreateForDevToolsFrontend(&delegate_);
  std::optional<base::Value> response_;
};

TEST_F(DevToolsEmbedderMessageDispatcherTest, UnknownMethod) {
  EXPECT_FALSE(Dispatch("noSuchMethod"));
  EXPECT_FALSE(response_.has_value());
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, NoParameters) {
  EXPECT_CALL(delegate_, CloseWindow());
  EXPECT_TRUE(Dispatch("closeWindow"));
  // A handler without a callback is acknowledged with a null response.
  ASSERT_TRUE(response_.has_value());
  EXPECT_EQ(*response_, base::Value());
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, NoParametersButArgument) {
  EXPECT_FALSE(Dispatch("closeWindow", base::ListValue().Append(true)));
  EXPECT_FALSE(response_.has_value());
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, RequiredParameters) {
  EXPECT_CALL(delegate_,
              SaveToFile("https://example.com/", "content", true, false));
  EXPECT_TRUE(Dispatch("save", base::ListValue()
                                   .Append("https://example.com/")
                                   .Append("content")
                                   .Append(true)
                                   .Append(false)));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, RequiredParameterMissing) {
  EXPECT_FALSE(Dispatch("save", base::ListValue()
                                    .Append("https://example.com/")
                                    .Append("content")
                                    .Append(true)));
  EXPECT_FALSE(Dispatch("save"));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, RequiredParameterNull) {
  EXPECT_FALSE(Dispatch("save", base::ListValue()
                                    .Append("https://example.com/")
                                    .Append(base::Value())
                                    .Append(true)
                                    .Append(false)));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, RequiredParameterOfWrongType) {
  EXPECT_FALSE(Dispatch("save", base::ListValue()
                                    .Append("https://example.com/")
                                    .Append("content")
                                    .Append("true")
                                    .Append(false)));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, ExtraArgument) {
  EXPECT_FALSE(Dispatch("save", base::ListValue()
                                    .Append("https://example.com/")
                                    .Append("content")
                                    .Append(true)
                                    .Append(false)
                                    .Append("extra")));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, OptionalParameter) {
  EXPECT_CALL(delegate_, OpenInNewTab("https://example.com/",
                                      Optional(std::string("frame-id"))));
  EXPECT_TRUE(Dispatch(
      "openInNewTab",
      base::ListValue().Append("https://example.com/").Append("frame-id")));
}

// JSON.stringify() turns an undefined argument into null.
TEST_F(DevToolsEmbedderMessageDispatcherTest, OptionalParameterNull) {
  EXPECT_CALL(delegate_,
              OpenInNewTab("https://example.com/", Eq(std::nullopt)));
  EXPECT_TRUE(Dispatch(
      "openInNewTab",
      base::ListValue().Append("https://example.com/").Append(base::Value())));
}

// A frontend from before the parameter existed does not send it at all.
TEST_F(DevToolsEmbedderMessageDispatcherTest, OptionalParameterOmitted) {
  EXPECT_CALL(delegate_,
              OpenInNewTab("https://example.com/", Eq(std::nullopt)));
  EXPECT_TRUE(Dispatch("openInNewTab",
                       base::ListValue().Append("https://example.com/")));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, OptionalParameterOfWrongType) {
  EXPECT_FALSE(
      Dispatch("openInNewTab",
               base::ListValue().Append("https://example.com/").Append(42)));
}

// Only trailing optional parameters may be omitted; a required parameter
// before them may not, and null is not a valid value for it either.
TEST_F(DevToolsEmbedderMessageDispatcherTest,
       RequiredParameterBeforeOptionalParameter) {
  EXPECT_FALSE(Dispatch("openInNewTab"));
  EXPECT_FALSE(
      Dispatch("openInNewTab", base::ListValue().Append(base::Value())));
  EXPECT_FALSE(
      Dispatch("openInNewTab",
               base::ListValue().Append(base::Value()).Append("frame-id")));
}

TEST_F(DevToolsEmbedderMessageDispatcherTest, ExtraArgumentAfterOptional) {
  EXPECT_FALSE(Dispatch("openInNewTab", base::ListValue()
                                            .Append("https://example.com/")
                                            .Append("frame-id")
                                            .Append("extra")));
}

// Handlers that respond through a callback parse their arguments the same way,
// and get to respond through the dispatch callback.
TEST_F(DevToolsEmbedderMessageDispatcherTest, HandlerWithCallback) {
  EXPECT_CALL(delegate_, SetIsDocked(_, true))
      .WillOnce([](DispatchCallback callback, bool) {
        base::Value response("docked");
        std::move(callback).Run(&response);
      });
  EXPECT_TRUE(Dispatch("setIsDocked", base::ListValue().Append(true)));
  ASSERT_TRUE(response_.has_value());
  EXPECT_EQ(*response_, base::Value("docked"));

  EXPECT_FALSE(Dispatch("setIsDocked"));
  EXPECT_FALSE(Dispatch("setIsDocked", base::ListValue().Append("true")));
  EXPECT_FALSE(
      Dispatch("setIsDocked", base::ListValue().Append(true).Append(false)));
}

}  // namespace
