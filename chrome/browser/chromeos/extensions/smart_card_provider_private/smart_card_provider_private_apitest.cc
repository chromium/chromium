// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/smart_card_provider_private.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/script_executor.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace scard_api = extensions::api::smart_card_provider_private;

using device::mojom::SmartCardDisposition;
using device::mojom::SmartCardError;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardResultPtr;
using device::mojom::SmartCardSuccess;
using testing::ElementsAre;

MATCHER_P(IsError, expected_error, "") {
  if (!arg->is_error()) {
    *result_listener << "is not an error";
    return false;
  }
  if (arg->get_error() != expected_error) {
    *result_listener << "expected " << expected_error << ", got "
                     << arg->get_error();
    return false;
  }
  return true;
}

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

  static constexpr char kTransactionJs[] =
      R"(
      let transactionActive = false;

      chrome.smartCardProviderPrivate.onBeginTransactionRequested.addListener(
          beginTransaction);

      function beginTransaction(requestId, scardHandle) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          return;
        }

        if (transactionActive === true) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "SHARING_VIOLATION");
          return;
        }

        transactionActive = true;

        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");
      }

      chrome.smartCardProviderPrivate.onEndTransactionRequested.addListener(
          endTransaction);

      function endTransaction(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          chrome.test.notifyFail(`Got EndTransaction on a dead connection.`);
          return;
        }

        if (transactionActive === false) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "NOT_TRANSACTED");
          chrome.test.notifyFail(
            `Got EndTransaction without an active transaction.`);
          return;
        }

        transactionActive = false;

        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");

        if (afterEndTransaction) {
          afterEndTransaction(disposition);
        }
      }
    )";

  static constexpr char kArrayEqualsJs[] =
      R"(
      const arrayEquals = (a, b) =>
        a.length === b.length &&
        a.every((v, i) => v === b[i]);
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

  void LoadFakeProviderExtension(
      std::initializer_list<std::string> js_snippets) {
    std::string background_js;
    for (auto& js_snippet : js_snippets) {
      background_js.append(js_snippet);
    }
    LoadFakeProviderExtension(background_js);
  }

  device::mojom::SmartCardCreateContextResultPtr CreateContext() {
    base::test::TestFuture<device::mojom::SmartCardCreateContextResultPtr>
        result_future;

    ProviderAPI().CreateContext(result_future.GetCallback());

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

  SmartCardProviderPrivateAPI& ProviderAPI() {
    return SmartCardProviderPrivateAPI::Get(*profile());
  }

  using ContextAndConnection =
      std::tuple<mojo::Remote<device::mojom::SmartCardContext>,
                 mojo::Remote<device::mojom::SmartCardConnection>>;

  ContextAndConnection CreateContextAndConnection() {
    ContextAndConnection result;
    auto context_result = CreateContext();
    if (!context_result->is_context()) {
      ADD_FAILURE() << "Failed to create a smart card context.";
      return ContextAndConnection();
    }
    mojo::Remote<device::mojom::SmartCardContext> context(
        std::move(context_result->get_context()));

    mojo::Remote<device::mojom::SmartCardConnection> connection =
        CreateConnection(*context.get());
    if (!connection.is_bound()) {
      ADD_FAILURE() << "Failed to create a smart card connection,";
      return ContextAndConnection();
    }

    return ContextAndConnection(std::move(context), std::move(connection));
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kAllowlistedExtensionID,
                                    "jofgjdphhceggjecimellaapdjjadibj");
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

 private:
  raw_ptr<const Extension, DanglingUntriaged> extension_;
};

class EventObserver : public EventRouter::TestObserver {
 public:
  size_t GetEventCount(const std::string& name) const {
    return event_count_.contains(name) ? event_count_.at(name) : 0;
  }
  void WaitForEventCount(const std::string& name, size_t count) {
    if (GetEventCount(name) >= count) {
      return;
    }
    expected_event_name_ = name;
    expected_event_count_ = count;
    run_loop_.Run();
  }

 private:
  void OnWillDispatchEvent(const Event& event) override {
    event_count_[event.event_name]++;
    if (expected_event_name_ == event.event_name &&
        GetEventCount(expected_event_name_) >= expected_event_count_) {
      run_loop_.Quit();
    }
  }
  void OnDidDispatchEventToProcess(const Event& event,
                                   int process_id) override {}

  std::map<std::string, size_t> event_count_;
  std::string expected_event_name_;
  size_t expected_event_count_;
  base::RunLoop run_loop_;
};

class DisconnectObserver {
 public:
  base::RepeatingClosure GetClosure() { return run_loop_.QuitClosure(); }
  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextNoProvider) {
  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextResponseTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension(R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          function(requestId){});
    )");

  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       EstablishContextResponseTimeoutTwice) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension(R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          function(requestId){});
    )");

  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kNoService));
  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CreateContext) {
  LoadFakeProviderExtension(kEstablishContextJs);
  auto context_result = CreateContext();
  EXPECT_TRUE(context_result->is_context());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CreateContextFails) {
  LoadFakeProviderExtension(R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, 0, "INTERNAL_ERROR");
      }
    )");

  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kInternalError));
}

// Tests that smartCardProviderPrivate.onReleaseContextRequested is emitted
// when a device::mojom::SmartCardContext is disconnected from its remote
// endpoint.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ContextMojoDisconnection) {
  LoadFakeProviderExtension(R"(
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
    )");

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
  LoadFakeProviderExtension(R"(
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
    )");
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
  LoadFakeProviderExtension(R"(
      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          establishContext);

      function establishContext(requestId) {
        let invalidScardContext = 0;
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, invalidScardContext, "SUCCESS");
      }
    )");

  EXPECT_THAT(CreateContext(), IsError(SmartCardError::kInternalError));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ListReaders) {
  LoadFakeProviderExtension({kEstablishContextJs,
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
    )"});

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

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ListReadersResponseTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs,
                             R"(
      chrome.smartCardProviderPrivate.onListReadersRequested.addListener(
          function(requestId, scardContext){});
    )"});

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      result_future;

  context->ListReaders(result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, GetStatusChange) {
  LoadFakeProviderExtension({kEstablishContextJs,
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
          // Just so that the test code can also check that
          // currentCount was correctly sent.
          state.eventCount = stateIn.currentCount + 1;
          state.atr = new Uint8Array([1,2,3,4,5]);
          readerStates.push(state);
        }

        chrome.smartCardProviderPrivate.reportGetStatusChangeResult(requestId,
            readerStates, "SUCCESS");
      }
    )"});

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
      state_in->current_count = 9u;
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
  EXPECT_EQ(state_out->event_count, 10u);
  EXPECT_EQ(state_out->answer_to_reset, std::vector<uint8_t>({1, 2, 3, 4, 5}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       GetStatusChangeNoProvider) {
  LoadFakeProviderExtension(kEstablishContextJs);

  EXPECT_THAT(GetStatusChange(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       GetStatusChangeResponseTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs,
                             R"(
    chrome.smartCardProviderPrivate.onGetStatusChangeRequested.addListener(
        function (requestId, scardContext, timeout, readerStatesIn) {});
  )"});

  EXPECT_THAT(GetStatusChange(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Connect) {
  LoadFakeProviderExtension({kEstablishContextJs,
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
    )"});

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

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectResponseTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs,
                             R"(
      chrome.smartCardProviderPrivate.onConnectRequested.addListener(
          function (requestId, scardContext, reader, shareMode,
              preferredProtocols) {});
    )"});

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

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Disconnect) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle || disposition != "UNPOWER_CARD") {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");
      }
    )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  SmartCardResultPtr result = result_future.Take();
  EXPECT_TRUE(result->is_success());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, DisconnectNoProvider) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, DisconnectTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
    chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
        function (requestId, scardHandle, disposition) {
          // Do nothing
        });
    )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> result_future;

  connection->Disconnect(device::mojom::SmartCardDisposition::kUnpower,
                         result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

// Tests that smartCardProviderPrivate.onDisconnectRequested is emitted
// when a device::mojom::SmartCardConnection is disconnected from its remote
// endpoint.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectionMojoDisconnection) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_HANDLE");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");
        chrome.test.notifyPass();
      }
      )"});

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

// If SmartCardConnection::Disconnect() was previously successfully called,
// do nothing once that SmartCardConnection is disconnected from its remote
// endpoint.
// Reasoning being that the PC/SC handle represented by this SmartCardConnection
// is no longer valid. There's nothing to cleanup at that point.
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectionApiDisconnectAndMojoDisconnection) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_HANDLE");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");
      }
      )"});

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  mojo::Remote<device::mojom::SmartCardConnection> connection =
      CreateConnection(*context.get());
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<SmartCardResultPtr> disconnect_result_future;
  connection->Disconnect(SmartCardDisposition::kLeave,
                         disconnect_result_future.GetCallback());

  ASSERT_TRUE(disconnect_result_future.Take()->is_success());

  DisconnectObserver disconnect_observer;
  ProviderAPI().SetDisconnectObserverForTesting(
      disconnect_observer.GetClosure());

  EventObserver event_observer;
  EventRouter* event_router =
      EventRouterFactory::GetForBrowserContext(profile());
  event_router->AddObserverForTesting(&event_observer);

  // Mojo disconnection from the remote endpoint should not cause
  // SmartCardProviderPrivateAPI to dispatch a
  // smartCardProviderPrivate.onDisconnectRequested event to the provider
  // extension since a successful PC/SC disconnection already took place.
  connection.reset();
  disconnect_observer.Wait();
  EXPECT_EQ(event_observer.GetEventCount(
                scard_api::OnDisconnectRequested::kEventName),
            0u);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Cancel) {
  LoadFakeProviderExtension({kEstablishContextJs,
                             R"(
      chrome.smartCardProviderPrivate.onCancelRequested.addListener(
          cancel);

      function cancel(requestId, scardContext) {
        if (scardContext != 123) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "SUCCESS");
      }
      )"});

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardResultPtr> result_future;

  context->Cancel(result_future.GetCallback());

  SmartCardResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_success());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CancelNoProvider) {
  LoadFakeProviderExtension(kEstablishContextJs);

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardResultPtr> result_future;

  context->Cancel(result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, CancelResponseTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, R"(
      chrome.smartCardProviderPrivate.onCancelRequested.addListener(
          function(requestId, scardContext){});
    )"});

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  base::test::TestFuture<device::mojom::SmartCardResultPtr> result_future;

  context->Cancel(result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

// A mojo::SmartCardContext receives a call while there's still another call
// waiting for an answer from the provider.
// The implementation should wait until the provider answers that pending
// request before it forwards him the next one.
//
// In this case, it's a ListReaders() followed by a Connect().
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ContextBusy) {
  LoadFakeProviderExtension(R"(
      let establishedContext = 0;

      let letListReadersProceed;
      let listReadersCanProceed = new Promise(function(resolve) {
        letListReadersProceed = resolve;
      });


      chrome.smartCardProviderPrivate.onEstablishContextRequested.addListener(
          function (requestId) {
        // Ensure we only give one context.
        if (establishedContext !== 0) {
          chrome.smartCardProviderPrivate.reportEstablishContextResult(
              requestId, establishedContext, "NO_MEMORY");
          return;
        }
        establishedContext = 123;
        chrome.smartCardProviderPrivate.reportEstablishContextResult(
            requestId, establishedContext, "SUCCESS");
      });

      chrome.smartCardProviderPrivate.onListReadersRequested.addListener(
          async function(requestId, scardContext){
        // Verify that the context is valid.
        if (establishedContext === 0 || scardContext !== establishedContext) {
          chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        await listReadersCanProceed;

        let readers = ["foo", "bar"];

        chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
            readers, "SUCCESS");
      });

      chrome.smartCardProviderPrivate.onConnectRequested.addListener(
          function (requestId, scardContext, reader, shareMode,
            preferredProtocols) {

        // Verify that the context is valid
        if (establishedContext === 0 || scardContext !== establishedContext) {
          chrome.smartCardProviderPrivate.reportConnectResult(requestId, 0,
              "", "INVALID_PARAMETER");
          return;
        }

        chrome.smartCardProviderPrivate.reportConnectResult(requestId, 987,
            "T1", "SUCCESS");
      });
    )");

  auto context_result = CreateContext();
  ASSERT_TRUE(context_result->is_context());
  mojo::Remote<device::mojom::SmartCardContext> context(
      std::move(context_result->get_context()));

  EventObserver event_observer;

  EventRouter* event_router =
      EventRouterFactory::GetForBrowserContext(profile());
  event_router->AddObserverForTesting(&event_observer);

  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      list_readers_future;

  base::test::TestFuture<device::mojom::SmartCardConnectResultPtr>
      connect_future;

  EXPECT_EQ(event_observer.GetEventCount(
                scard_api::OnListReadersRequested::kEventName),
            0u);
  context->ListReaders(list_readers_future.GetCallback());
  context.FlushForTesting();
  // The ListReaders request should go straight away since the context is free.
  EXPECT_EQ(event_observer.GetEventCount(
                scard_api::OnListReadersRequested::kEventName),
            1u);

  context->Connect("foo", device::mojom::SmartCardShareMode::kShared,
                   device::mojom::SmartCardProtocols::New(),
                   connect_future.GetCallback());
  context.FlushForTesting();
  // The Connect request should not have been sent since the context is still
  // busy with the ListReaders that hasn't been answered yet.
  EXPECT_EQ(
      event_observer.GetEventCount(scard_api::OnConnectRequested::kEventName),
      0u);

  // Let the ListReaders call finish.
  {
    static constexpr char kScript[] =
        R"(
           letListReadersProceed();
           chrome.test.sendScriptResult('ok');
         )";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension()->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    ASSERT_TRUE(result.is_string());
    EXPECT_EQ("ok", result.GetString());
  }

  {
    device::mojom::SmartCardListReadersResultPtr result =
        list_readers_future.Take();
    EXPECT_TRUE(result->is_readers());
  }

  // Now that the ListReaders call has finished the queued Connect request
  // should finally go through.
  event_observer.WaitForEventCount(scard_api::OnConnectRequested::kEventName,
                                   1u);

  {
    device::mojom::SmartCardConnectResultPtr result = connect_future.Take();
    EXPECT_TRUE(result->is_success());
  }

  event_router->RemoveObserverForTesting(&event_observer);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       ConnectionSharesContextFate) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      let letListReadersProceed;
      let listReadersCanProceed = new Promise(function(resolve) {
        letListReadersProceed = resolve;
      });

      chrome.smartCardProviderPrivate.onListReadersRequested.addListener(
          async function(requestId, scardContext){
        if (scardContext !== 123) {
          chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
              readerStates, "INVALID_PARAMETER");
          return;
        }

        console.log('listreaders will wait...');
        await listReadersCanProceed;
        console.log('proceeding with listreaders');
        let readers = ["foo-reader"];

        chrome.smartCardProviderPrivate.reportListReadersResult(requestId,
            readers, "SUCCESS");
      });

      chrome.smartCardProviderPrivate.onReleaseContextRequested.addListener(
          function(requestId, scardContext) {
        if (scardContext != 123) {
          chrome.smartCardProviderPrivate.reportReleaseContextResult(requestId,
              "INVALID_PARAMETER");
          return;
        }
        chrome.smartCardProviderPrivate.reportReleaseContextResult(
            requestId, "SUCCESS");
      });
      )"});

  DisconnectObserver disconnect_observer;

  ProviderAPI().SetDisconnectObserverForTesting(
      disconnect_observer.GetClosure());

  EventObserver event_observer;
  EventRouter* event_router =
      EventRouterFactory::GetForBrowserContext(profile());
  event_router->AddObserverForTesting(&event_observer);

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  // ListReaders() won't be answered until told so.
  // Thus its request will remain pending.
  base::test::TestFuture<device::mojom::SmartCardListReadersResultPtr>
      list_readers_future;
  context->ListReaders(list_readers_future.GetCallback());
  context.FlushForTesting();
  EXPECT_EQ(event_observer.GetEventCount(
                scard_api::OnListReadersRequested::kEventName),
            1u);

  EXPECT_TRUE(connection.is_connected());

  // Queues a ReleaseContext() to the provider due to the pending ListReaders()
  // response.
  context.reset();
  disconnect_observer.Wait();
  EXPECT_EQ(event_observer.GetEventCount(
                scard_api::OnReleaseContextRequested::kEventName),
            0u);

  // Lost of the SmartCardContext also causes the SmartCardConnection it created
  // to get disconnected, so it won't be able to send any requests.
  connection.FlushForTesting();
  EXPECT_FALSE(connection.is_connected());

  // Let the ListReaders call finish.
  {
    static constexpr char kScript[] =
        R"(
           letListReadersProceed();
           chrome.test.sendScriptResult('ok');
         )";
    base::Value result = BackgroundScriptExecutor::ExecuteScript(
        profile(), extension()->id(), kScript,
        BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
    ASSERT_TRUE(result.is_string());
    EXPECT_EQ("ok", result.GetString());
  }

  // After ListReaders is done, ReleaseContext should go through.
  event_observer.WaitForEventCount(
      scard_api::OnReleaseContextRequested::kEventName, 1u);

  // The ListReaders callback never had the chance to get called since the
  // mojom::SmartCardContext was reset before that request finished.
  EXPECT_FALSE(list_readers_future.IsReady());

  ProviderAPI().SetDisconnectObserverForTesting(base::RepeatingClosure());
  event_router->RemoveObserverForTesting(&event_observer);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Transmit) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kArrayEqualsJs,
                             R"(
      chrome.smartCardProviderPrivate.onTransmitRequested.addListener(
          transmit);

      function transmit(requestId, scardHandle, protocol, data) {

        const inputArray = new Uint8Array(data);
        const expectedInputArray = new Uint8Array([3, 2, 1]);

        if (scardHandle !== validHandle || protocol != "T1"
            || !arrayEquals(inputArray, expectedInputArray)) {
          chrome.smartCardProviderPrivate.reportDataResult(requestId,
            new Uint8Array().buffer,
            "INVALID_PARAMETER");
          return;
        }

        let responseData = new Uint8Array([1, 100, 255]);

        chrome.smartCardProviderPrivate.reportDataResult(requestId,
          responseData.buffer, "SUCCESS");
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->Transmit(device::mojom::SmartCardProtocol::kT1, {3u, 2u, 1u},
                       result_future.GetCallback());

  auto result = result_future.Take();
  ASSERT_TRUE(result->is_data());

  EXPECT_EQ(result->get_data(), std::vector<uint8_t>({1u, 100u, 255u}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, TransmitTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      chrome.smartCardProviderPrivate.onTransmitRequested.addListener(
          function (requestId, scardHandle, protocol, data) {
            // Do nothing.
          });
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->Transmit(device::mojom::SmartCardProtocol::kT1,
                       std::vector<uint8_t>({3u, 2u, 1u}),
                       result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Control) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kArrayEqualsJs,
                             R"(
      chrome.smartCardProviderPrivate.onControlRequested.addListener(
          control);

      function control(requestId, scardHandle, controlCode, data) {

        const inputArray = new Uint8Array(data);
        const expectedInputArray = new Uint8Array([3, 2, 1]);

        if (scardHandle !== validHandle || controlCode !== 111
            || !arrayEquals(inputArray, expectedInputArray)) {
          chrome.smartCardProviderPrivate.reportDataResult(requestId,
            new Uint8Array().buffer,
            "INVALID_PARAMETER");
          return;
        }

        let responseData = new Uint8Array([1, 100, 255]);

        chrome.smartCardProviderPrivate.reportDataResult(requestId,
          responseData.buffer, "SUCCESS");
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->Control(111u, {3u, 2u, 1u}, result_future.GetCallback());

  device::mojom::SmartCardDataResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_data());

  EXPECT_EQ(result->get_data(), std::vector<uint8_t>({1u, 100u, 255u}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, ControlTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, R"(
      chrome.smartCardProviderPrivate.onControlRequested.addListener(
          function (requestId, scardHandle, controlCode, data) {
            // Do nothing.
          });
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->Control(111u, std::vector<uint8_t>({3u, 2u, 1u}),
                      result_future.GetCallback());

  EXPECT_THAT(result_future.Take(), IsError(SmartCardError::kNoService));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, GetAttrib) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, R"(
      chrome.smartCardProviderPrivate.onGetAttribRequested.addListener(
          getAttrib);

      function getAttrib(requestId, scardHandle, attribId) {
        if (scardHandle !== validHandle || attribId !== 111) {
          chrome.smartCardProviderPrivate.reportDataResult(requestId,
            new Uint8Array().buffer,
            "INVALID_PARAMETER");
          return;
        }

        let responseData = new Uint8Array([1, 100, 255]);

        chrome.smartCardProviderPrivate.reportDataResult(requestId,
          responseData.buffer, "SUCCESS");
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->GetAttrib(111u, result_future.GetCallback());

  device::mojom::SmartCardDataResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_data());

  EXPECT_EQ(result->get_data(), std::vector<uint8_t>({1u, 100u, 255u}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, GetAttribTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, R"(
      chrome.smartCardProviderPrivate.onGetAttribRequested.addListener(
          function (requestId, scardHandle, attribId) {
            // Do nothing.
          });
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardDataResultPtr> result_future;

  connection->GetAttrib(111u, result_future.GetCallback());

  device::mojom::SmartCardDataResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, SetAttrib) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kArrayEqualsJs,
                             R"(
      chrome.smartCardProviderPrivate.onSetAttribRequested.addListener(
          setAttrib);

      function setAttrib(requestId, scardHandle, attribId, data) {

        const inputArray = new Uint8Array(data);
        const expectedInputArray = new Uint8Array([3, 2, 1]);

        if (scardHandle !== validHandle || attribId != 111
            || !arrayEquals(inputArray, expectedInputArray)) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          return;
        }

        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardResultPtr> result_future;

  connection->SetAttrib(111u, std::vector<uint8_t>({3u, 2u, 1u}),
                        result_future.GetCallback());

  device::mojom::SmartCardResultPtr result = result_future.Take();
  EXPECT_TRUE(result->is_success());
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, SetAttribTimeout) {
  ProviderAPI().SetResponseTimeLimitForTesting(base::Seconds(1));

  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, R"(
      chrome.smartCardProviderPrivate.onSetAttribRequested.addListener(
          function (requestId, scardHandle, attribId, data) {
            // Do nothing.
          });
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardResultPtr> result_future;

  connection->SetAttrib(111u, std::vector<uint8_t>({3u, 2u, 1u}),
                        result_future.GetCallback());

  device::mojom::SmartCardResultPtr result = result_future.Take();
  ASSERT_TRUE(result->is_error());
  EXPECT_EQ(result->get_error(), SmartCardError::kNoService);
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest, Status) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs,
                             R"(
      chrome.smartCardProviderPrivate.onStatusRequested.addListener(
          status);

      function status(requestId, scardHandle) {
        if (scardHandle !== validHandle) {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          return;
        }

        chrome.smartCardProviderPrivate.reportStatusResult(requestId,
          "FooReader", "SPECIFIC", "T1", new Uint8Array([3, 2, 1]),
          "SUCCESS");
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardStatusResultPtr> result_future;

  connection->Status(result_future.GetCallback());

  auto result = result_future.Take();
  ASSERT_TRUE(result->is_status());

  device::mojom::SmartCardStatusPtr& status = result->get_status();
  EXPECT_EQ(status->reader_name, "FooReader");
  EXPECT_EQ(status->state, device::mojom::SmartCardConnectionState::kSpecific);
  EXPECT_EQ(status->protocol, device::mojom::SmartCardProtocol::kT1);
  EXPECT_EQ(status->answer_to_reset, std::vector<uint8_t>({3u, 2u, 1u}));
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       BeginTransactionAndDropMojoRemote) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kTransactionJs,
                             R"(
      function afterEndTransaction(disposition) {
        if (disposition !== "LEAVE_CARD") {
          chrome.test.notifyFail(`Wrong disposition: ${disposition}`);
        }
        chrome.test.notifyPass();
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr>
      result_future;

  connection->BeginTransaction(result_future.GetCallback());

  auto result = result_future.Take();
  ASSERT_TRUE(result->is_transaction());

  // mojo disconnection of the SmartCardTransaction should trigger a
  // onEndTransactionRequested event to the provider.
  ResultCatcher result_catcher;
  {
    mojo::AssociatedRemote<device::mojom::SmartCardTransaction> transaction(
        std::move(result->get_transaction()));

    EXPECT_TRUE(transaction.is_connected());
  }
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       BeginAndEndTransaction) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kTransactionJs,
                             R"(
      function afterEndTransaction(disposition) {
        if (disposition !== "UNPOWER_CARD") {
          chrome.test.notifyFail(`Wrong disposition: ${disposition}`);
        }
        chrome.test.notifyPass();
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr>
      result_future;

  connection->BeginTransaction(result_future.GetCallback());

  auto result = result_future.Take();
  ASSERT_TRUE(result->is_transaction());

  mojo::AssociatedRemote<device::mojom::SmartCardTransaction> transaction(
      std::move(result->get_transaction()));
  EXPECT_TRUE(transaction.is_connected());

  ResultCatcher result_catcher;
  base::test::TestFuture<device::mojom::SmartCardResultPtr> end_result_future;
  transaction->EndTransaction(SmartCardDisposition::kUnpower,
                              end_result_future.GetCallback());
  EXPECT_TRUE(end_result_future.Take()->is_success());
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

// If a SmartCardConnection remote is dropped, its SmartCardTransaction mojo
// connection (if any) will be dropped as well.
// The interface implementations are expected to clean up appropriately by
// calling first EndTransaction() and then Disconnect().
IN_PROC_BROWSER_TEST_F(SmartCardProviderPrivateApiTest,
                       TransactionSharesConnectionFate) {
  LoadFakeProviderExtension({kEstablishContextJs, kConnectJs, kTransactionJs,
                             R"(
      function afterEndTransaction(disposition) {
        if (disposition !== "LEAVE_CARD") {
          chrome.test.notifyFail(`Wrong disposition: ${disposition}`);
        }
        chrome.test.notifyPass();
      }

      chrome.smartCardProviderPrivate.onDisconnectRequested.addListener(
          disconnect);

      function disconnect(requestId, scardHandle, disposition) {
        if (scardHandle !== validHandle || disposition != "LEAVE_CARD") {
          chrome.smartCardProviderPrivate.reportPlainResult(requestId,
            "INVALID_PARAMETER");
          return;
        }
        validHandle = 0;
        chrome.smartCardProviderPrivate.reportPlainResult(requestId,
          "SUCCESS");

        if (transactionActive === true) {
          chrome.test.notifyFail(`Disconnected with an active transaction.`);
        }
      }
      )"});

  auto [context, connection] = CreateContextAndConnection();
  ASSERT_TRUE(connection.is_bound());

  base::test::TestFuture<device::mojom::SmartCardTransactionResultPtr>
      result_future;

  connection->BeginTransaction(result_future.GetCallback());

  auto result = result_future.Take();
  ASSERT_TRUE(result->is_transaction());

  mojo::AssociatedRemote<device::mojom::SmartCardTransaction> transaction(
      std::move(result->get_transaction()));
  EXPECT_TRUE(transaction.is_connected());

  ResultCatcher result_catcher;

  // Losing an SmartCardConnection mojo connection should also trigger
  // mojo disconnection of the SmartCardTransaction
  connection.reset();
  transaction.FlushForTesting();
  EXPECT_FALSE(transaction.is_connected());

  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
