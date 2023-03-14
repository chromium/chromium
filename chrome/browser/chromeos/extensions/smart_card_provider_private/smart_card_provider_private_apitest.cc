// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

using device::mojom::SmartCardError;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardResultPtr;
using device::mojom::SmartCardSuccess;
using testing::ElementsAre;

namespace extensions {

class SmartCardProviderPrivateApiTest : public ExtensionApiTest {
 public:
  static constexpr char kEstablishContextJs[] =
      R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, 123, "SUCCESS");
      }
    )";

  static constexpr char kConnectJs[] =
      R"(
      let validHandle = 0;

      chrome.smartCardProviderPrivate.onConnectRequested.addListener(
          connect);

      function connect(requestId, scardContext, reader,
          shareMode, preferredProtocols) {
        if (scardContext != 123
            || validHandle !== 0) {
          chrome.smartCardProviderPrivate.reportGetStatusChangeResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }
        validHandle = 987;
        chrome.smartCardProviderPrivate.reportConnectResult(requestId,
            validHandle, "T1", "SUCCESS");
      }
    )";

  void LoadFakeProviderExtension(const std::string& background_js) {
    TestExtensionDir test_dir;
    constexpr char kManifest[] =
        R"({
             "key": "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA2nI64+TVbJNYUfte1hEwrWpjgiH3ucfKZ12NC6IT/Pm2pQdjR/3alrdW+rCkYbs0KmfUymb0GOE7fwZ0Gs+EqfxoKmwJaaZiv86GXEkPJctDvjqrJRUrKvM6aXZEkTQeaFLQVY9NDk3grSZzvC365l3c4zRN3A2i8KMWzB9HRQzKnN49zjgcTTu5DERYTzbJZBd0m9Ln1b3x3UVkVgoTUq7DexGMcOq1KYz0VHrFRo/LN1yJvECFmBb2pdl40g4UHq3UqrWDDInZZJ3sr01EePxYYwimMFsGnvH6sz8wHC09rXZ+w1YFYjsQ3P/3Bih1q/NdZ0aop3MEOCbHb4gipQIDAQAB",
             "name": "Fake Smart Card Provider",
             "version": "0.1",
             "manifest_version": 2,
             "background": { "scripts": ["background.js"] },
             "permissions": [ "smartCardProviderPrivate" ]
           })";
    test_dir.WriteManifest(kManifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background_js);
    extension_ = LoadExtension(test_dir.UnpackedPath());
    ASSERT_TRUE(extension_);
  }

  device::mojom::SmartCardCreateContextResultPtr CreateContext() {
    SmartCardProviderPrivateAPI& scard_provider_api =
        SmartCardProviderPrivateAPI::Get(*profile());

    base::test::TestFuture<device::mojom::SmartCardCreateContextResultPtr>
        result_future;

    scard_provider_api.CreateContext(result_future.GetCallback());

    return result_future.Take();
  }

  // Create a context and call GetStatusChange() on it with valid
  // parameters.
  device::mojom::SmartCardStatusChangeResultPtr GetStatusChange() {
    auto context_result = CreateContext();
    EXPECT_TRUE(context_result->is_context());
    mojo::Remote<device::mojom::SmartCardContext> context(
        std::move(context_result->get_context()));

    base::test::TestFuture<device::mojom::SmartCardStatusChangeResultPtr>
        result_future;

    {
      std::vector<device::mojom::SmartCardReaderStateInPtr> states_in;
      {
        auto state_in = device::mojom::SmartCardReaderStateIn::New();
        state_in->reader = "foo";
        state_in->current_state =
            device::mojom::SmartCardReaderStateFlags::New();
        state_in->current_state->unaware = true;
        state_in->current_state->ignore = false;
        states_in.push_back(std::move(state_in));
      }

      context->GetStatusChange(base::Seconds(1), std::move(states_in),
                               result_future.GetCallback());
    }

    return result_future.Take();
  }

  const Extension* extension() const { return extension_; }

  mojo::Remote<device::mojom::SmartCardConnection> CreateConnection(
      device::mojom::SmartCardContext& context) {
    base::test::TestFuture<device::mojom::SmartCardConnectResultPtr>
        result_future;

    auto preferred_protocols = device::mojom::SmartCardProtocols::New();
    preferred_protocols->t1 = true;

    context.Connect("foo-reader", device::mojom::SmartCardShareMode::kShared,
                    std::move(preferred_protocols),
                    result_future.GetCallback());

    device::mojom::SmartCardConnectResultPtr result = result_future.Take();
    if (result->is_error()) {
      ADD_FAILURE() << "Connect failed: " << result->get_error();
      return mojo::Remote<device::mojom::SmartCardConnection>();
    }

    device::mojom::SmartCardConnectSuccessPtr success =
        std::move(result->get_success());

    mojo::Remote<device::mojom::SmartCardConnection> connection(
        std::move(success->connection));
    EXPECT_TRUE(connection.is_connected());
    return connection;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    "jofgjdphhceggjecimellaapdjjadibj");
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

 private:
  base::raw_ptr<const Extension, DanglingUntriaged> extension_;
};

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextNoProvider) {
  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_error());
  EXPECT_EQ(context_result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextResponseTimeout) {
  SmartCardProviderPrivateAPI& scard_provider_api =
      SmartCardProviderPrivateAPI::Get(*profile());

  scard_provider_api.SetResponseTimeLimitForTesting(base::Seconds(1));

  constexpr char kBackgroundJs[] =
      R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          function(requestId){});
    )";
  LoadFakeProviderExtension(kBackgroundJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_error());
  EXPECT_EQ(context_result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CreateContext) {
  LoadFakeProviderExtension(kEstablishContextJs);
  auto context_result = CreateContext();
  EXPECT_TRUE(context_result->is_context());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CreateContextFails) {
  constexpr char kBackgroundJs[] =
      R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, 0, "INTERNAL_ERROR");
      }
    )";
  LoadFakeProviderExtension(kBackgroundJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_error());
  EXPECT_EQ(context_result->get_error(), SmartCardError::kInternalError);
}

// Tests that smartCardProviderPrivate.onReleaseContextRequested is emitted
// when a device::mojom::SmartCardContext is disconnected from its remote
// endpoint.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ContextMojoDisconnection) {
  constexpr char kBackgroundJs[] =
      R"(
      let establishedContext = 0;
      let establishContextCalled = false;

      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        if (establishContextCalled) {
          chrome.test.fail("EstablishContext called more than once");
          chrome.smartCardProviderPrivate.reportEstablishContextResult(
              requestId, establishedContext, "INTERNAL_ERROR");
          return;
        }

        establishContextCalled = true;
        establishedContext = 123;
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, establishedContext, "SUCCESS");
      }

      chrome.smartCardProviderPrivate.onReleaseContextRequested.addListener(
          releaseContext);

      function releaseContext(requestId, scardContext) {
        if (scardContext != establishedContext || scardContext === 0) {
          chrome.smartCardProviderPrivate.reportReleaseContextResult(requestId,
              "INVALID_PARAMETER");
          return;
        }

        establishedContext = 0;

        chrome.smartCardProviderPrivate.reportReleaseContextResult(
            requestId, "SUCCESS");

        chrome.test.notifyPass();
      }
    )";
  LoadFakeProviderExtension(kBackgroundJs);

  ResultCatcher result_catcher;
  {
    auto context_result = CreateContext();
    ASSERT_TRUE(context_result->is_context());
    mojo::Remote<device::mojom::SmartCardContext> context(
        std::move(context_result->get_context()));
  }
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// If we receive an scard_context from an unknown request, we should release it
// automatically to avoid "leaking" it in the provider side.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextUnknown) {
  constexpr char kBackgroundJs[] =
      R"(
      // An scard context that SmartCardProviderPrivateAPI did not ask for.
      let unwantedScardContext = 333;

      chrome.smartCardProviderPrivate.onReleaseContextRequested.addListener(
          releaseContext);

      function releaseContext(requestId, scardContext) {
        if (scardContext !== unwantedScardContext) {
          chrome.smartCardProviderPrivate.reportReleaseContextResult(requestId,
              "INVALID_PARAMETER");
          return;
        }
        chrome.test.notifyPass();
      }

        function reportUnknownEstablishContext() {
          // Some arbitrary request id that SmartCardProviderPrivateAPI did
          // not generate.
          let unknownRequestId = 222;
          chrome.smartCardProviderPrivate.reportEstablishContextResult(
            unknownRequestId, unwantedScardContext, "SUCCESS");
        };
    )";
  LoadFakeProviderExtension(kBackgroundJs);
  ResultCatcher result_catcher;
  BackgroundScriptExecutor::ExecuteScriptAsync(
      profile(), extension()->id(), "reportUnknownEstablishContext();");
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// In case the provider reports a successful EstablishContext request
// but gives an invalid scard_context value for it, the browser should
// consider the request as failed, ignoring this scard_context.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextInvalid) {
  constexpr char kBackgroundJs[] =
      R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        let invalidScardContext = 0;
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, invalidScardContext, "SUCCESS");
      }
    )";
  LoadFakeProviderExtension(kBackgroundJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_error());
  EXPECT_EQ(context_result->get_error(), SmartCardError::kInternalError);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ListReaders) {
  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onListReadersRequested.addListener(
          listReaders);

      function listReaders(requestId, scardContext) {
        if (scardContext != 123) {
          chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        let readers = ["foo", "bar"];

        chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
            readers, "SUCCESS");
      }
    )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      result_future;

  context->ListReaders(result_future.GetCallback());

  device::mojom::SmartCardListReadersResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_readers());

  std::vector<std::string>& readers = result->get_readers();

  EXPECT_THAT(readers, ElementsAre("foo", "bar"));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ListReadersNoProvider) {
  LoadFakeProviderExtension(kEstablishContextJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      result_future;

  context->ListReaders(result_future.GetCallback());

  device::mojom::SmartCardListReadersResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ListReadersResponseTimeout) {
  SmartCardProviderPrivateAPI& scard_provider_api =
      SmartCardProviderPrivateAPI::Get(*profile());
  scard_provider_api.SetResponseTimeLimitForTesting(base::Seconds(1));

  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onListReadersRequested.addListener(
          function(requestId, scardContext){});
    )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      result_future;

  context->ListReaders(result_future.GetCallback());

  device::mojom::SmartCardListReadersResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, GetStatusChange) {
  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onGetStatusChangeRequested.addListener(
          getStatusChange);

      function getStatusChange(requestId, scardContext, timeout,
                               readerStatesIn) {
        let readerStates = [];

        if (scardContext != 123 || timeout.milliseconds !== 1000) {
          chrome.smartCardProviderPrivate.reportGetStatusChangeResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        for (const stateIn of readerStatesIn) {
          let state = {};
          state.reader = stateIn.reader;
          state.eventState = {"present": true};
          state.atr = new Uint8Array([1,2,3,4,5]);
          readerStates.push(state);
        }

        chrome.smartCardProviderPrivate.reportGetStatusChangeResult(requestId,
            readerStates, "SUCCESS");
      }
    )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardStatusChangeResultPtr>
      result_future;

  {
    std::vector<device::mojom::SmartCardReaderStateInPtr> states_in;
    {
      auto state_in = device::mojom::SmartCardReaderStateIn::New();
      state_in->reader = "foo";
      state_in->current_state = device::mojom::SmartCardReaderStateFlags::New();
      state_in->current_state->unaware = true;
      state_in->current_state->ignore = false;
      states_in.push_back(std::move(state_in));
    }

    context->GetStatusChange(base::Seconds(1), std::move(states_in),
                             result_future.GetCallback());
  }

  device::mojom::SmartCardStatusChangeResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_reader_states());

  std::vector<device::mojom::SmartCardReaderStateOutPtr>& states_out =
      result->get_reader_states();

  ASSERT_EQ(states_out.size(), size_t(1));
  auto& state_out = states_out.at(0);
  EXPECT_EQ(state_out->reader, "foo");
  EXPECT_FALSE(state_out->event_state->unaware);
  EXPECT_TRUE(state_out->event_state->present);
  EXPECT_EQ(state_out->answer_to_reset, std::vector<uint8_t>({1, 2, 3, 4, 5}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       GetStatusChangeNoProvider) {
  LoadFakeProviderExtension(kEstablishContextJs);

  device::mojom::SmartCardStatusChangeResultPtr result = GetStatusChange();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       GetStatusChangeResponseTimeout) {
  SmartCardProviderPrivateAPI& scard_provider_api =
      SmartCardProviderPrivateAPI::Get(*profile());
  scard_provider_api.SetResponseTimeLimitForTesting(base::Seconds(1));

  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onGetStatusChangeRequested.addListener(
          function (requestId, scardContext, timeout, readerStatesIn) {});
    )");
  LoadFakeProviderExtension(background_js);

  device::mojom::SmartCardStatusChangeResultPtr result = GetStatusChange();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Connect) {
  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onConnectRequested.addListener(
          connect);

      function connect(requestId, scardContext, reader,
          shareMode, preferredProtocols) {
        if (scardContext != 123
            || reader !== "foo-reader"
            || shareMode !== "SHARED"
            || preferredProtocols.t0 !== false
            || preferredProtocols.t1 !== true
            || preferredProtocols.raw !== false) {
          chrome.smartCardProviderPrivate.reportGetStatusChangeResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        chrome.smartCardProviderPrivate.reportConnectResult(requestId, 987,
            "T1", "SUCCESS");
      }
    )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr>
      result_future;

  auto preferred_protocols = device::mojom::SmartCardProtocols::New();
  preferred_protocols->t0 = false;
  preferred_protocols->t1 = true;
  preferred_protocols->raw = false;

  context->Connect("foo-reader", device::mojom::SmartCardShareMode::kShared,
                   std::move(preferred_protocols), result_future.GetCallback());

  device::mojom::SmartCardConnectResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_success());

  device::mojom::SmartCardConnectSuccessPtr success =
      std::move(result->get_success());
  EXPECT_EQ(success->active_protocol, device::mojom::SmartCardProtocol::kT1);

  mojo::Remote<device::mojom::SmartCardConnection> connection(
      std::move(success->connection));
  EXPECT_TRUE(connection.is_connected());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ConnectNoProvider) {
  LoadFakeProviderExtension(kEstablishContextJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr>
      result_future;

  auto preferred_protocols = device::mojom::SmartCardProtocols::New();
  preferred_protocols->t1 = true;

  context->Connect("foo-reader", device::mojom::SmartCardShareMode::kShared,
                   std::move(preferred_protocols), result_future.GetCallback());

  device::mojom::SmartCardConnectResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectResponseTimeout) {
  SmartCardProviderPrivateAPI& scard_provider_api =
      SmartCardProviderPrivateAPI::Get(*profile());
  scard_provider_api.SetResponseTimeLimitForTesting(base::Seconds(1));

  std::string background_js(kEstablishContextJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onConnectRequested.addListener(
          function (requestId, scardContext, reader, shareMode,
              preferredProtocols) {});
      )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr>
      result_future;

  auto preferred_protocols = device::mojom::SmartCardProtocols::New();
  preferred_protocols->t1 = true;

  context->Connect("foo-reader", device::mojom::SmartCardShareMode::kShared,
                   std::move(preferred_protocols), result_future.GetCallback());

  device::mojom::SmartCardConnectResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Disconnect) {
  std::string background_js(kEstablishContextJs);
  background_js.append(kConnectJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle || disposition != "UNPOWER_CARD") {
          chrome.smartCardProviderPrivate.reportDisconnectResult(requestId,
            "INVALID_PARAMETER");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportDisconnectResult(requestId,
          "SUCCESS");
      }
      )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  mojo::Remote<device::mojom::SmartCardConnection> connection =
      CreateConnection(*context.get());
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  SmartCardResultPtr result = result_future.Take();
  EXPECT_TRUE(result->is_success());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, DisconnectNoProvider) {
  std::string background_js(kEstablishContextJs);
  background_js.append(kConnectJs);
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  mojo::Remote<device::mojom::SmartCardConnection> connection =
      CreateConnection(*context.get());
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  SmartCardResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, DisconnectTimeout) {
  SmartCardProviderPrivateAPI& scard_provider_api =
      SmartCardProviderPrivateAPI::Get(*profile());
  scard_provider_api.SetResponseTimeLimitForTesting(base::Seconds(1));

  std::string background_js(kEstablishContextJs);
  background_js.append(kConnectJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        // Do nothing
      }
      )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  mojo::Remote<device::mojom::SmartCardConnection> connection =
      CreateConnection(*context.get());
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  SmartCardResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

// Tests that smartCardProviderPrivate.onDisconnectRequested is emitted
// when a device::mojom::SmartCardConnection is disconnected from its remote
// endpoint.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectionMojoDisconnection) {
  std::string background_js(kEstablishContextJs);
  background_js.append(kConnectJs);
  background_js.append(
      R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportDisconnectResult(requestId,
            "INVALID_HANDLE");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportDisconnectResult(requestId,
          "SUCCESS");
        chrome.test.notifyPass();
      }
      )");
  LoadFakeProviderExtension(background_js);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  ResultCatcher result_catcher;
  {
    mojo::Remote<device::mojom::SmartCardConnection> connection =
        CreateConnection(*context.get());
    ASSERT_TRUE(connection.is_bound());
  }
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
