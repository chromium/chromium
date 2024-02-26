// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_update_win.h"

#include <windows.h>

#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/i18n/string_search.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_path_override.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_simple_task_runner.h"
#include "base/version.h"
#include "base/win/atl.h"
#include "base/win/registry.h"
#include "chrome/browser/google/switches.h"
#include "chrome/common/chrome_version.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/updater/app/server/win/updater_legacy_idl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"

using ::testing::_;
using ::testing::AllOfArray;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::IsEmpty;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::Values;

namespace {

// GMock's HasSubstr does not support std::u16string. Hence redefine it as a
// generic matcher.
MATCHER_P(HasSubstr, str, "") {
  return arg.find(str) != arg.npos;
}

MATCHER_P(HasSubstrCaseInsensitive, str, "") {
  return base::i18n::StringSearchIgnoringCaseAndAccents(str, arg, nullptr,
                                                        nullptr);
}

class MockUpdateCheckDelegate : public UpdateCheckDelegate {
 public:
  MockUpdateCheckDelegate() : weak_ptr_factory_(this) {}

  MockUpdateCheckDelegate(const MockUpdateCheckDelegate&) = delete;
  MockUpdateCheckDelegate& operator=(const MockUpdateCheckDelegate&) = delete;

  base::WeakPtr<UpdateCheckDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD1(OnUpdateCheckComplete, void(const std::u16string&));
  MOCK_METHOD2(OnUpgradeProgress, void(int, const std::u16string&));
  MOCK_METHOD1(OnUpgradeComplete, void(const std::u16string&));
  MOCK_METHOD3(OnError,
               void(GoogleUpdateErrorCode,
                    const std::u16string&,
                    const std::u16string&));

 private:
  base::WeakPtrFactory<UpdateCheckDelegate> weak_ptr_factory_;
};

// An interface that exposes a factory method for creating an IGoogleUpdate3Web
// instance.
class GoogleUpdateFactory {
 public:
  virtual ~GoogleUpdateFactory() {}
  virtual HRESULT Create(
      Microsoft::WRL::ComPtr<IGoogleUpdate3Web>* google_update) = 0;
};

class MockCurrentState : public CComObjectRootEx<CComSingleThreadModel>,
                         public ICurrentState {
 public:
  BEGIN_COM_MAP(MockCurrentState)
    COM_INTERFACE_ENTRY(ICurrentState)
  END_COM_MAP()

  MockCurrentState() {}

  MockCurrentState(const MockCurrentState&) = delete;
  MockCurrentState& operator=(const MockCurrentState&) = delete;

  // Adds an expectation for get_completionMessage that will return the given
  // message any number of times.
  void ExpectCompletionMessage(const std::u16string& completion_message) {
    completion_message_ = completion_message;
    EXPECT_CALL(*this, get_completionMessage(_))
        .WillRepeatedly(
            ::testing::Invoke(this, &MockCurrentState::GetCompletionMessage));
  }

  HRESULT GetCompletionMessage(BSTR* completion_message) {
    *completion_message = SysAllocString(base::as_wcstr(completion_message_));
    return S_OK;
  }

  // Adds an expectation for get_availableVersion that will return the given
  // version any number of times.
  void ExpectAvailableVersion(const std::u16string& available_version) {
    available_version_ = available_version;
    EXPECT_CALL(*this, get_availableVersion(_))
        .WillRepeatedly(
            ::testing::Invoke(this, &MockCurrentState::GetAvailableVersion));
  }

  HRESULT GetAvailableVersion(BSTR* available_version) {
    *available_version = SysAllocString(base::as_wcstr(available_version_));
    return S_OK;
  }

  // ICurrentState:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_stateValue,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_availableVersion,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_bytesDownloaded,
                             HRESULT(ULONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_totalBytesToDownload,
                             HRESULT(ULONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_downloadTimeRemainingMs,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_nextRetryTime,
                             HRESULT(ULONGLONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installProgress,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installTimeRemainingMs,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_isCanceled,
                             HRESULT(VARIANT_BOOL *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_errorCode,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_extraCode1,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_completionMessage,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installerResultCode,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_installerResultExtraCode1,
                             HRESULT(LONG *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallLaunchCommandLine,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallUrl,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_postInstallAction,
                             HRESULT(LONG *));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

 private:
  std::u16string completion_message_;
  std::u16string available_version_;
};

// A mock IAppWeb that can run callers of get_currentState through a sequence of
// pre-programmed states registered via the various Push*State methods.
class MockApp : public CComObjectRootEx<CComSingleThreadModel>, public IAppWeb {
 public:
  BEGIN_COM_MAP(MockApp)
    COM_INTERFACE_ENTRY(IAppWeb)
  END_COM_MAP()

  MockApp() {
    // Connect get_currentState so that each call will go to GetNextState.
    ON_CALL(*this, get_currentState(_))
        .WillByDefault(::testing::Invoke(this, &MockApp::GetNextState));
  }

  MockApp(const MockApp&) = delete;
  MockApp& operator=(const MockApp&) = delete;

  // IAppWeb:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_appId,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentVersionWeb,
                             HRESULT(IDispatch **));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_nextVersionWeb,
                             HRESULT(IDispatch **));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_command,
                             HRESULT(BSTR, IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             cancel,
                             HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentState,
                             HRESULT(IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             launch,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             uninstall,
                             HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_serverInstallDataIndex,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             put_serverInstallDataIndex,
                             HRESULT(BSTR));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb.
  void PushState(CurrentState state) { MakeNextState(state); }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for an ERROR state.
  void PushErrorState(LONG error_code,
                      const std::u16string& completion_message,
                      LONG installer_result_code) {
    CComObject<MockCurrentState>* mock_state = MakeNextState(STATE_ERROR);
    EXPECT_CALL(*mock_state, get_errorCode(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(error_code), Return(S_OK)));
    mock_state->ExpectCompletionMessage(completion_message);
    if (installer_result_code != -1) {
      EXPECT_CALL(*mock_state, get_installerResultCode(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(installer_result_code),
                                Return(S_OK)));
    }
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for an UPDATE_AVAILABLE state.
  void PushUpdateAvailableState(const std::u16string& new_version) {
    MakeNextState(STATE_UPDATE_AVAILABLE)->ExpectAvailableVersion(new_version);
  }

  // Adds a MockCurrentState to the back of the sequence to be returned by the
  // mock IAppWeb for a DOWNLOADING or INSTALLING state.
  void PushProgressiveState(CurrentState state, int progress) {
    CComObject<MockCurrentState>* mock_state = MakeNextState(state);
    if (state == STATE_DOWNLOADING) {
      const ULONG kTotalBytes = 1024;
      ULONG bytes_down = static_cast<double>(kTotalBytes) * progress / 100.0;
      EXPECT_CALL(*mock_state, get_totalBytesToDownload(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(kTotalBytes), Return(S_OK)));
      EXPECT_CALL(*mock_state, get_bytesDownloaded(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(bytes_down), Return(S_OK)));
    } else if (state == STATE_INSTALLING) {
      EXPECT_CALL(*mock_state, get_installProgress(_))
          .WillRepeatedly(DoAll(SetArgPointee<0>(progress), Return(S_OK)));
    } else {
      ADD_FAILURE() << "unsupported state " << state;
    }
  }

 private:
  // Returns a new MockCurrentState that will be returned by the mock IAppWeb's
  // get_currentState method.
  CComObject<MockCurrentState>* MakeNextState(CurrentState state) {
    CComObject<MockCurrentState>* mock_state = nullptr;
    // The new object's refcount is held at zero until it is released from the
    // simulator in GetNextState.
    EXPECT_EQ(S_OK, CComObject<MockCurrentState>::CreateInstance(&mock_state));
    EXPECT_CALL(*mock_state, get_stateValue(_))
        .WillRepeatedly(DoAll(SetArgPointee<0>(state), Return(S_OK)));
    states_.push(mock_state);
    // Tell the app to expect this state.
    EXPECT_CALL(*this, get_currentState(_)).InSequence(state_sequence_);
    return mock_state;
  }

  // An implementation of IAppWeb::get_currentState that advances the
  // IGoogleUpdate3Web simulator through a series of states.
  HRESULT GetNextState(IDispatch** current_state) {
    EXPECT_FALSE(states_.empty());
    *current_state = states_.front();
    // Give a reference to the caller.
    (*current_state)->AddRef();
    states_.pop();
    return S_OK;
  }

  // The states returned by the MockApp when probed.
  base::queue<raw_ptr<CComObject<MockCurrentState>, CtnExperimental>> states_;

  // A gmock sequence under which a series of get_CurrentState expectations are
  // evaluated.
  Sequence state_sequence_;
};

// A mock IAppBundleWeb that can handle a single call to createInstalledApp
// followed by get_appWeb.
class MockAppBundle : public CComObjectRootEx<CComSingleThreadModel>,
                      public IAppBundleWeb {
 public:
  BEGIN_COM_MAP(MockAppBundle)
    COM_INTERFACE_ENTRY(IAppBundleWeb)
  END_COM_MAP()

  MockAppBundle() {}

  MockAppBundle(const MockAppBundle&) = delete;
  MockAppBundle& operator=(const MockAppBundle&) = delete;

  // IAppBundleWeb:
  MOCK_METHOD4_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createApp,
                             HRESULT(BSTR, BSTR, BSTR, BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createInstalledApp,
                             HRESULT(BSTR));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createAllInstalledApps,
                             HRESULT());
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_displayLanguage,
                             HRESULT(BSTR *));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             put_displayLanguage,
                             HRESULT(BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             put_parentHWND,
                             HRESULT(ULONG_PTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_length,
                             HRESULT(int *));
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_appWeb,
                             HRESULT(int, IDispatch **));
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             initialize,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             checkForUpdate,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             download,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             install,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             pause,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             resume,
                             HRESULT());
  MOCK_METHOD0_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             cancel,
                             HRESULT());
  MOCK_METHOD2_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             downloadPackage,
                             HRESULT(BSTR, BSTR));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             get_currentState,
                             HRESULT(VARIANT *));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

  // Returns a MockApp for the given |app_guid| that will be returned by this
  // instance's get_appWeb method. The returned instance is only valid for use
  // in setting up expectations until a consumer obtains it via get_appWeb, at
  // which time it is owned by the consumer.
  CComObject<MockApp>* MakeApp(const wchar_t* app_guid) {
    // The bundle will be called on to create the installed app.
    EXPECT_CALL(*this, createInstalledApp(StrEq(app_guid)))
        .WillOnce(Return(S_OK));

    CComObject<MockApp>* mock_app = nullptr;
    EXPECT_EQ(S_OK, CComObject<MockApp>::CreateInstance(&mock_app));

    // Give mock_app_bundle a ref to the app which it will return when asked.
    // Note: to support multiple apps, get_appWeb expectations should use
    // successive indices.
    mock_app->AddRef();
    EXPECT_CALL(*this, get_appWeb(0, _))
        .WillOnce(DoAll(SetArgPointee<1>(mock_app),
                        Return(S_OK)));

    return mock_app;
  }
};

// A mock IGoogleUpdate3Web that can handle a call to initialize and
// createAppBundleWeb by consumers.
class MockGoogleUpdate : public CComObjectRootEx<CComSingleThreadModel>,
                         public IGoogleUpdate3Web {
 public:
  BEGIN_COM_MAP(MockGoogleUpdate)
    COM_INTERFACE_ENTRY(IGoogleUpdate3Web)
  END_COM_MAP()

  MockGoogleUpdate() {}

  MockGoogleUpdate(const MockGoogleUpdate&) = delete;
  MockGoogleUpdate& operator=(const MockGoogleUpdate&) = delete;

  // IGoogleUpdate3Web:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             createAppBundleWeb,
                             HRESULT(IDispatch**));

  // IDispatch:
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfoCount,
                             HRESULT(UINT *));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetTypeInfo,
                             HRESULT(UINT, LCID, ITypeInfo **));
  MOCK_METHOD5_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetIDsOfNames,
                             HRESULT(REFIID, LPOLESTR *, UINT, LCID, DISPID *));
  MOCK_METHOD8_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             Invoke,
                             HRESULT(DISPID, REFIID, LCID, WORD, DISPPARAMS *,
                                     VARIANT *, EXCEPINFO *, UINT *));

  // Returns a MockAppBundle that will be returned by this instance's
  // createAppBundleWeb method. The returned instance is only valid for use in
  // setting up expectations until a consumer obtains it via createAppBundleWeb,
  // at which time it is owned by the consumer.
  CComObject<MockAppBundle>* MakeAppBundle() {
    CComObject<MockAppBundle>* mock_app_bundle = nullptr;
    EXPECT_EQ(S_OK,
              CComObject<MockAppBundle>::CreateInstance(&mock_app_bundle));
    EXPECT_CALL(*mock_app_bundle, initialize())
        .WillOnce(Return(S_OK));
    // Give this instance a ref to the bundle which it will return when created.
    mock_app_bundle->AddRef();
    EXPECT_CALL(*this, createAppBundleWeb(_))
        .WillOnce(DoAll(SetArgPointee<0>(mock_app_bundle), Return(S_OK)));
    return mock_app_bundle;
  }
};

// A mock factory for creating an IGoogleUpdate3Web instance.
class MockGoogleUpdateFactory : public GoogleUpdateFactory {
 public:
  MockGoogleUpdateFactory() {}

  MockGoogleUpdateFactory(const MockGoogleUpdateFactory&) = delete;
  MockGoogleUpdateFactory& operator=(const MockGoogleUpdateFactory&) = delete;

  MOCK_METHOD1(Create, HRESULT(Microsoft::WRL::ComPtr<IGoogleUpdate3Web>*));

  // Returns a mock IGoogleUpdate3Web object that will be returned by the
  // factory.
  CComObject<MockGoogleUpdate>* MakeServerMock() {
    CComObject<MockGoogleUpdate>* mock_google_update = nullptr;
    EXPECT_EQ(S_OK, CComObject<MockGoogleUpdate>::CreateInstance(
                        &mock_google_update));
    // Give the factory this updater. Do not add a ref, as the factory will add
    // one when it hands out its instance.
    EXPECT_CALL(*this, Create(_))
        .InSequence(sequence_)
        .WillOnce(DoAll(SetArgPointee<0>(mock_google_update), Return(S_OK)));
    return mock_google_update;
  }

 private:
  Sequence sequence_;
};

}  // namespace

// A test fixture that can simulate the IGoogleUpdate3Web API via Google Mock.
// Individual tests must wire up the factories by a call to one of the
// PrepareSimulator methods. The family of Push*State methods are then used to
// configure the set of states to be simulated.
class GoogleUpdateWinTest : public ::testing::TestWithParam<bool> {
 public:
  GoogleUpdateWinTest(const GoogleUpdateWinTest&) = delete;
  GoogleUpdateWinTest& operator=(const GoogleUpdateWinTest&) = delete;

  static void SetUpTestCase() {
    ui::win::CreateATLModuleIfNeeded();
    // Configure all mock functions that return HRESULT to return failure.
    ::testing::DefaultValue<HRESULT>::Set(E_FAIL);
  }

  static void TearDownTestCase() { ::testing::DefaultValue<HRESULT>::Clear(); }

 protected:
  GoogleUpdateWinTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        task_runner_current_default_handle_(task_runner_),
        system_level_install_(GetParam()),
        scoped_install_details_(system_level_install_, 0) {}

  void SetUp() override {
    ::testing::TestWithParam<bool>::SetUp();

    // Override these paths so that they can be found after the registry
    // override manager is in place.
    base::FilePath temp;
    base::PathService::Get(base::DIR_PROGRAM_FILES, &temp);
    program_files_override_.reset(
        new base::ScopedPathOverride(base::DIR_PROGRAM_FILES, temp));
    base::PathService::Get(base::DIR_PROGRAM_FILESX86, &temp);
    program_files_x86_override_.reset(
        new base::ScopedPathOverride(base::DIR_PROGRAM_FILESX86, temp));
    base::PathService::Get(base::DIR_LOCAL_APP_DATA, &temp);
    local_app_data_override_.reset(
        new base::ScopedPathOverride(base::DIR_LOCAL_APP_DATA, temp));

    // Override the registry so that tests can freely push state to it.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));

    // Chrome is installed.
    const HKEY root =
        system_level_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    base::win::RegKey key(root, kClients, KEY_WRITE | KEY_WOW64_32KEY);
    ASSERT_EQ(ERROR_SUCCESS,
              key.CreateKey(kChromeGuid, KEY_WRITE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"pv",
                             base::ASCIIToWide(CHROME_VERSION_STRING).c_str()));
    ASSERT_EQ(ERROR_SUCCESS,
              key.Create(root, kClientState, KEY_WRITE | KEY_WOW64_32KEY));
    ASSERT_EQ(ERROR_SUCCESS,
              key.CreateKey(kChromeGuid, KEY_WRITE | KEY_WOW64_32KEY));
    base::FilePath dir_exe;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &dir_exe));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"UninstallString",
                             dir_exe.AppendASCII(CHROME_VERSION_STRING)
                                 .Append(installer::kInstallerDir)
                                 .Append(L"setup.exe")
                                 .value()
                                 .c_str()));
    ASSERT_EQ(ERROR_SUCCESS,
              key.WriteValue(L"UninstallArguments", L"--uninstall"));

    // Provide an IGoogleUpdate3Web class factory so that this test can provide
    // a mocked-out instance.
    SetGoogleUpdateFactoryForTesting(
        base::BindRepeating(&GoogleUpdateFactory::Create,
                            base::Unretained(&mock_google_update_factory_)));

    // Compute a newer version.
    base::Version current_version(CHROME_VERSION_STRING);
    new_version_ = base::ASCIIToUTF16(base::StringPrintf(
        "%u.%u.%u.%u", current_version.components()[0],
        current_version.components()[1], current_version.components()[2] + 1,
        current_version.components()[3]));

    SetUpdateDriverTaskRunnerForTesting(task_runner_.get());
  }

  // Creates app bundle and app mocks that will be used to simulate Google
  // Update.
  void MakeGoogleUpdateMocks(CComObject<MockAppBundle>** mock_app_bundle,
                             CComObject<MockApp>** mock_app) {
    CComObject<MockGoogleUpdate>* google_update =
        mock_google_update_factory_.MakeServerMock();
    CComObject<MockAppBundle>* app_bundle = google_update->MakeAppBundle();
    CComObject<MockApp>* app = app_bundle->MakeApp(kChromeGuid);

    if (mock_app_bundle)
      *mock_app_bundle = app_bundle;
    if (mock_app)
      *mock_app = app;
  }

  void TearDown() override {
    SetUpdateDriverTaskRunnerForTesting(nullptr);
    // Remove the test's IGoogleUpdate on-demand update class factory.
    SetGoogleUpdateFactoryForTesting(GoogleUpdate3ClassFactory());
    ::testing::TestWithParam<bool>::TearDown();
  }

  static const wchar_t kClients[];
  static const wchar_t kClientState[];
  static const wchar_t kChromeGuid[];

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::SingleThreadTaskRunner::CurrentDefaultHandle
      task_runner_current_default_handle_;
  bool system_level_install_;
  install_static::ScopedInstallDetails scoped_install_details_;
  std::unique_ptr<base::ScopedPathOverride> program_files_override_;
  std::unique_ptr<base::ScopedPathOverride> program_files_x86_override_;
  std::unique_ptr<base::ScopedPathOverride> local_app_data_override_;
  registry_util::RegistryOverrideManager registry_override_manager_;

  // A mock object, the OnUpdateCheckCallback method of which will be invoked
  // each time the update check machinery invokes the given UpdateCheckCallback.
  StrictMock<MockUpdateCheckDelegate> mock_update_check_delegate_;

  // A mock object that provides a GoogleUpdate3ClassFactory by which the test
  // fixture's IGoogleUpdate3Web simulator is provided to the update check
  // machinery.
  StrictMock<MockGoogleUpdateFactory> mock_google_update_factory_;

  // The new version that the fixture will pretend is available.
  std::u16string new_version_;
};

//  static
const wchar_t GoogleUpdateWinTest::kClients[] =
    L"Software\\Google\\Update\\Clients";
const wchar_t GoogleUpdateWinTest::kClientState[] =
    L"Software\\Google\\Update\\ClientState";
const wchar_t GoogleUpdateWinTest::kChromeGuid[] =
    L"{8A69D345-D564-463c-AFF1-A69D9E530F96}";

// Test that an update check fails with the proper error code if Chrome isn't in
// one of the expected install directories.
TEST_P(GoogleUpdateWinTest, InvalidInstallDirectory) {
  // Override FILE_EXE so that it looks like the test is running from somewhere
  // other than where Chrome is installed.
  base::FilePath file_exe;
  base::FilePath dir_temp;
  ASSERT_TRUE(base::PathService::Get(base::FILE_EXE, &file_exe));
  ASSERT_TRUE(base::PathService::Get(base::DIR_TEMP, &dir_temp));
  base::ScopedPathOverride file_exe_override(
      base::FILE_EXE, dir_temp.Append(file_exe.BaseName()),
      /*is_absolute=*/true, /*create=*/false);

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY, _, _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            CANNOT_UPGRADE_CHROME_IN_THIS_DIRECTORY);
}

// Test the case where the GoogleUpdate class can't be created for an update
// check.
TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForCheck) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND);
}

// Test the case where the GoogleUpdate class can't be created for an upgrade.
TEST_P(GoogleUpdateWinTest, NoGoogleUpdateForUpgrade) {
  // The factory should be called upon: let it fail.
  EXPECT_CALL(mock_google_update_factory_, Create(_));

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND, _, _));
  BeginUpdateCheck(std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND);
}

// Test the case where the GoogleUpdate class returns an error when an update
// check is started.
TEST_P(GoogleUpdateWinTest, FailUpdateCheck) {
  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, nullptr);

  // checkForUpdate will fail.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(E_FAIL));

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR, _, _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            GOOGLE_UPDATE_ONDEMAND_CLASS_REPORTED_ERROR);
  EXPECT_EQ(GetLastUpdateState()->hresult, E_FAIL);
}

// Test the case where the GoogleUpdate class reports that updates are disabled
// by Group Policy.
TEST_P(GoogleUpdateWinTest, UpdatesDisabledByPolicy) {
  static const HRESULT GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY = 0x80040813;

  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushErrorState(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY,
                           u"disabled by policy", -1);

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_DISABLED_BY_POLICY, _, _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_DISABLED_BY_POLICY);
}

// Test the case where the GoogleUpdate class reports that manual updates are
// disabled by Group Policy, but that automatic updates are enabled.
TEST_P(GoogleUpdateWinTest, ManualUpdatesDisabledByPolicy) {
  static const HRESULT GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL =
      0x8004081f;

  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushErrorState(GOOPDATE_E_APP_UPDATE_DISABLED_BY_POLICY_MANUAL,
                           u"manual updates disabled by policy", -1);

  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY, _, _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            GOOGLE_UPDATE_DISABLED_BY_POLICY_AUTO_ONLY);
}

// Test an update check where no update is available.
TEST_P(GoogleUpdateWinTest, UpdateCheckNoUpdate) {
  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushState(STATE_NO_UPDATE);

  EXPECT_CALL(mock_update_check_delegate_,
              OnUpdateCheckComplete(IsEmpty()));  // new_version
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_NO_ERROR);
  EXPECT_EQ(GetLastUpdateState()->new_version, u"");
}

// Test an update check where an update is available.
TEST_P(GoogleUpdateWinTest, UpdateCheckUpdateAvailable) {
  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushUpdateAvailableState(new_version_);

  EXPECT_CALL(mock_update_check_delegate_, OnUpdateCheckComplete(new_version_));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_NO_ERROR);
  EXPECT_EQ(GetLastUpdateState()->new_version, new_version_);
}

// Test a successful upgrade.
TEST_P(GoogleUpdateWinTest, UpdateInstalled) {
  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));
  // Expect the bundle to be called on to start the install.
  EXPECT_CALL(*mock_app_bundle, install())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushUpdateAvailableState(new_version_);
  mock_app->PushState(STATE_WAITING_TO_DOWNLOAD);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 0);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 75);
  mock_app->PushState(STATE_WAITING_TO_INSTALL);
  mock_app->PushProgressiveState(STATE_INSTALLING, 50);
  mock_app->PushState(STATE_INSTALL_COMPLETE);

  {
    InSequence callback_sequence;
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(0, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(12, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(37, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(50, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(75, new_version_));
    EXPECT_CALL(mock_update_check_delegate_, OnUpgradeComplete(new_version_));
  }
  BeginUpdateCheck(std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_NO_ERROR);
  EXPECT_EQ(GetLastUpdateState()->new_version, new_version_);
}

// Test a failed upgrade where Google Update reports that the installer failed.
TEST_P(GoogleUpdateWinTest, UpdateFailed) {
  const std::u16string error(u"It didn't work.");
  static const HRESULT GOOPDATEINSTALL_E_INSTALLER_FAILED = 0x80040902;
  static const int kInstallerError = 12;

  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate())
      .WillOnce(Return(S_OK));
  // Expect the bundle to be called on to start the install.
  EXPECT_CALL(*mock_app_bundle, install())
      .WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushUpdateAvailableState(new_version_);
  mock_app->PushState(STATE_WAITING_TO_DOWNLOAD);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 0);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 75);
  mock_app->PushState(STATE_WAITING_TO_INSTALL);
  mock_app->PushProgressiveState(STATE_INSTALLING, 50);
  mock_app->PushErrorState(GOOPDATEINSTALL_E_INSTALLER_FAILED, error,
                           kInstallerError);

  {
    InSequence callback_sequence;
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(0, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(12, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(37, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(50, new_version_));
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(75, new_version_));
    EXPECT_CALL(
        mock_update_check_delegate_,
        OnError(GOOGLE_UPDATE_ERROR_UPDATING, HasSubstr(error), new_version_));
  }
  BeginUpdateCheck(std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_ERROR_UPDATING);
  EXPECT_EQ(GetLastUpdateState()->new_version, new_version_);
  EXPECT_EQ(GetLastUpdateState()->hresult, GOOPDATEINSTALL_E_INSTALLER_FAILED);
  ASSERT_TRUE(GetLastUpdateState()->installer_exit_code);
  EXPECT_EQ(GetLastUpdateState()->installer_exit_code.value(), kInstallerError);
}

// Test that a retry after a USING_EXTERNAL_UPDATER failure succeeds.
TEST_P(GoogleUpdateWinTest, RetryAfterExternalUpdaterError) {
  static const HRESULT GOOPDATE_E_APP_USING_EXTERNAL_UPDATER = 0xa043081d;

  CComObject<MockAppBundle>* mock_app_bundle =
      mock_google_update_factory_.MakeServerMock()->MakeAppBundle();

  // The first attempt will fail in createInstalledApp indicating that an update
  // is already in progress.
  Sequence bundle_seq;
  EXPECT_CALL(*mock_app_bundle, createInstalledApp(StrEq(kChromeGuid)))
      .InSequence(bundle_seq)
      .WillOnce(Return(GOOPDATE_E_APP_USING_EXTERNAL_UPDATER));

  // Expect a retry on the same instance.
  EXPECT_CALL(*mock_app_bundle, createInstalledApp(StrEq(kChromeGuid)))
      .InSequence(bundle_seq)
      .WillOnce(Return(S_OK));

  // See MakeApp() for an explanation of this:
  CComObject<MockApp>* mock_app = nullptr;
  EXPECT_EQ(S_OK, CComObject<MockApp>::CreateInstance(&mock_app));
  mock_app->AddRef();
  EXPECT_CALL(*mock_app_bundle, get_appWeb(0, _))
      .WillOnce(DoAll(SetArgPointee<1>(mock_app), Return(S_OK)));

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate()).WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushState(STATE_NO_UPDATE);

  // Expect the update check to succeed.
  EXPECT_CALL(mock_update_check_delegate_,
              OnUpdateCheckComplete(IsEmpty()));  // new_version
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_NO_ERROR);
  EXPECT_EQ(GetLastUpdateState()->new_version, u"");
}

TEST_P(GoogleUpdateWinTest, UpdateInstalledMultipleDelegates) {
  CComObject<MockAppBundle>* mock_app_bundle = nullptr;
  CComObject<MockApp>* mock_app = nullptr;
  MakeGoogleUpdateMocks(&mock_app_bundle, &mock_app);

  // Expect the bundle to be called on to start the update.
  EXPECT_CALL(*mock_app_bundle, checkForUpdate()).WillOnce(Return(S_OK));
  // Expect the bundle to be called on to start the install.
  EXPECT_CALL(*mock_app_bundle, install()).WillOnce(Return(S_OK));

  mock_app->PushState(STATE_INIT);
  mock_app->PushState(STATE_CHECKING_FOR_UPDATE);
  mock_app->PushUpdateAvailableState(new_version_);
  mock_app->PushState(STATE_WAITING_TO_DOWNLOAD);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 0);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 25);
  mock_app->PushProgressiveState(STATE_DOWNLOADING, 75);
  mock_app->PushState(STATE_WAITING_TO_INSTALL);
  mock_app->PushProgressiveState(STATE_INSTALLING, 50);
  mock_app->PushState(STATE_INSTALL_COMPLETE);

  StrictMock<MockUpdateCheckDelegate> mock_update_check_delegate_2;
  {
    InSequence callback_sequence;
    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(0, new_version_));
    EXPECT_CALL(mock_update_check_delegate_2,
                OnUpgradeProgress(0, new_version_));

    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(12, new_version_));
    EXPECT_CALL(mock_update_check_delegate_2,
                OnUpgradeProgress(12, new_version_));

    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(37, new_version_));
    EXPECT_CALL(mock_update_check_delegate_2,
                OnUpgradeProgress(37, new_version_));

    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(50, new_version_));
    EXPECT_CALL(mock_update_check_delegate_2,
                OnUpgradeProgress(50, new_version_));

    EXPECT_CALL(mock_update_check_delegate_,
                OnUpgradeProgress(75, new_version_));
    EXPECT_CALL(mock_update_check_delegate_2,
                OnUpgradeProgress(75, new_version_));

    EXPECT_CALL(mock_update_check_delegate_, OnUpgradeComplete(new_version_));
    EXPECT_CALL(mock_update_check_delegate_2, OnUpgradeComplete(new_version_));
  }
  BeginUpdateCheck(std::string(), true, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  BeginUpdateCheck(std::string(), true, 0,
                   mock_update_check_delegate_2.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_NO_ERROR);
  EXPECT_EQ(GetLastUpdateState()->new_version, new_version_);
}

// Test the case where the GoogleUpdate class responds with error provided
// via the command line with specified hresult and GoogleUpdateErrorCode.
TEST_P(GoogleUpdateWinTest, SimulateHresultWithErrorCode) {
  base::test::ScopedCommandLine commandline;
  // Simulate GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND (3).
  commandline.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kSimulateUpdateErrorCode, "3");
  // Simulate WININET_E_INCORRECT_HANDLE_TYPE (0x80072ef2).
  commandline.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kSimulateUpdateHresult, "0x80072ef2");

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND,
                      AllOfArray({HasSubstrCaseInsensitive(u"error code 3:"),
                                  HasSubstrCaseInsensitive(u"0x80072EF2")}),
                      _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code,
            GOOGLE_UPDATE_ONDEMAND_CLASS_NOT_FOUND);
}

// Test the case where the GoogleUpdate class responds with error provided
// via the command line with a specific hresult.
TEST_P(GoogleUpdateWinTest, SimulateHresultOnly) {
  base::test::ScopedCommandLine commandline;
  // Simulate WININET_E_INCORRECT_HANDLE_TYPE (0x80072ef2).
  commandline.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kSimulateUpdateHresult, "0x80072ef2");

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ERROR_UPDATING,
                      AllOfArray({HasSubstrCaseInsensitive(u"error code 7:"),
                                  HasSubstrCaseInsensitive(u"0x80072EF2")}),
                      _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_ERROR_UPDATING);
}

// Test the case where the GoogleUpdate class responds with error provided
// via the command line without specific hresult.
TEST_P(GoogleUpdateWinTest, SimulateHresultDefault) {
  base::test::ScopedCommandLine commandline;
  commandline.GetProcessCommandLine()->AppendSwitch(
      switches::kSimulateUpdateHresult);

  // Expect the appropriate error when the on-demand class cannot be created.
  EXPECT_CALL(mock_update_check_delegate_,
              OnError(GOOGLE_UPDATE_ERROR_UPDATING,
                      AllOfArray({HasSubstrCaseInsensitive(u"error code 7:"),
                                  HasSubstrCaseInsensitive(u"0x80004005")}),
                      _));
  BeginUpdateCheck(std::string(), false, 0,
                   mock_update_check_delegate_.AsWeakPtr());
  task_runner_->RunUntilIdle();
  ASSERT_TRUE(GetLastUpdateState());
  EXPECT_EQ(GetLastUpdateState()->error_code, GOOGLE_UPDATE_ERROR_UPDATING);
}

INSTANTIATE_TEST_SUITE_P(UserLevel, GoogleUpdateWinTest, Values(false));

INSTANTIATE_TEST_SUITE_P(SystemLevel, GoogleUpdateWinTest, Values(true));
