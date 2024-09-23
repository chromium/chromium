// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/browser/spellchecker/spell_check_initialization_host_impl.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_common.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/common/spellcheck_result.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/sync/base/command_line_switches.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
#include "components/spellcheck/common/spellcheck_features.h"
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

using content::BrowserContext;
using content::RenderProcessHost;

class SpellcheckServiceBrowserTest : public InProcessBrowserTest,
                                     public spellcheck::mojom::SpellChecker {
 public:
#if BUILDFLAG(IS_WIN)
  explicit SpellcheckServiceBrowserTest(
      bool use_browser_spell_checker = false) {
    if (!use_browser_spell_checker) {
      // Tests were designed assuming Hunspell dictionary used and many fail
      // when Windows spellcheck is enabled.
      disable_browser_spell_checker_.emplace();
    }
  }
#else
  SpellcheckServiceBrowserTest() = default;
#endif

  SpellcheckServiceBrowserTest(const SpellcheckServiceBrowserTest&) = delete;
  SpellcheckServiceBrowserTest& operator=(const SpellcheckServiceBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    renderer_ = std::make_unique<content::MockRenderProcessHost>(GetContext());
    renderer_->Init();
    prefs_ = user_prefs::UserPrefs::Get(GetContext());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Sync causes the SpellcheckService to be instantiated (and initialized)
    // during startup. However, several tests rely on control over when exactly
    // the SpellcheckService gets created (e.g. by calling
    // GetEnableSpellcheckState() after InitSpellcheck(), which will wait
    // forever if the service already existed). So disable sync of the custom
    // dictionary for these tests.
    command_line->AppendSwitch(syncer::kDisableSync);
    SpellcheckService::OverrideBinderForTesting(base::BindRepeating(
        &SpellcheckServiceBrowserTest::Bind, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    SpellcheckService::OverrideBinderForTesting(base::NullCallback());
    receiver_.reset();
    prefs_ = nullptr;
    renderer_.reset();
  }

  RenderProcessHost* GetRenderer() const { return renderer_.get(); }

  BrowserContext* GetContext() const {
    return static_cast<BrowserContext*>(browser()->profile());
  }

  PrefService* GetPrefs() const { return prefs_; }

  void InitSpellcheck(bool enable_spellcheck,
                      const std::string& single_dictionary,
                      const std::string& multiple_dictionaries) {
    prefs_->SetBoolean(spellcheck::prefs::kSpellCheckEnable, enable_spellcheck);
    prefs_->SetString(spellcheck::prefs::kSpellCheckDictionary,
                      single_dictionary);
    base::Value::List dictionaries_value;
    const std::vector<std::string> str_list =
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const std::string& str : str_list) {
      dictionaries_value.Append(str);
    }
    prefs_->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                    std::move(dictionaries_value));

    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(renderer_->GetBrowserContext());

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    if (spellcheck::UseBrowserSpellChecker()) {
      // If the Windows native spell checker is in use, initialization is async.
      RunTestRunLoop();
    }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    ASSERT_NE(nullptr, spellcheck);
  }

  void EnableSpellcheck(bool enable_spellcheck) {
    prefs_->SetBoolean(spellcheck::prefs::kSpellCheckEnable, enable_spellcheck);
  }

  void ChangeCustomDictionary() {
    SpellcheckService* spellcheck =
        SpellcheckServiceFactory::GetForContext(renderer_->GetBrowserContext());
    ASSERT_NE(nullptr, spellcheck);

    SpellcheckCustomDictionary::Change change;
    change.RemoveWord("1");
    change.AddWord("2");
    change.AddWord("3");

    spellcheck->OnCustomDictionaryChanged(change);
  }

  void SetMultiLingualDictionaries(const std::string& multiple_dictionaries) {
    base::Value::List dictionaries_value;
    const std::vector<std::string> str_list =
        base::SplitString(multiple_dictionaries, ",", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    for (const std::string& str : str_list) {
      dictionaries_value.Append(str);
    }
    prefs_->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                    std::move(dictionaries_value));
  }

  std::string GetMultilingualDictionaries() {
    const base::Value::List& list_value =
        prefs_->GetList(spellcheck::prefs::kSpellCheckDictionaries);
    std::vector<std::string_view> dictionaries;
    for (const auto& item_value : list_value) {
      EXPECT_TRUE(item_value.is_string());
      dictionaries.push_back(item_value.GetString());
    }
    return base::JoinString(dictionaries, ",");
  }

  void SetAcceptLanguages(const std::string& accept_languages) {
    prefs_->SetString(language::prefs::kAcceptLanguages, accept_languages);
  }

  bool GetEnableSpellcheckState(bool initial_state = false) {
    spellcheck_enabled_state_ = initial_state;
    RunTestRunLoop();
    EXPECT_TRUE(initialize_spellcheck_called_);
    EXPECT_TRUE(bound_connection_closed_);
    return spellcheck_enabled_state_;
  }

  bool GetCustomDictionaryChangedState() {
    RunTestRunLoop();
    EXPECT_TRUE(bound_connection_closed_);
    return custom_dictionary_changed_called_;
  }

 private:
  // Spins a RunLoop to deliver the Mojo SpellChecker request flow.
  void RunTestRunLoop() {
    bound_connection_closed_ = false;
    initialize_spellcheck_called_ = false;
    custom_dictionary_changed_called_ = false;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Binds requests for the SpellChecker interface.
  void Bind(mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver) {
    receiver_.reset();
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(
        base::BindOnce(&SpellcheckServiceBrowserTest::BoundConnectionClosed,
                       base::Unretained(this)));
  }

  // The requester closes (disconnects) when done.
  void BoundConnectionClosed() {
    bound_connection_closed_ = true;
    receiver_.reset();
    if (quit_)
      std::move(quit_).Run();
  }

  // spellcheck::mojom::SpellChecker:
  void Initialize(
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
      const std::vector<std::string>& custom_words,
      bool enable) override {
    initialize_spellcheck_called_ = true;
    spellcheck_enabled_state_ = enable;
  }

  void CustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override {
    custom_dictionary_changed_called_ = true;
    EXPECT_EQ(1u, words_removed.size());
    EXPECT_EQ(2u, words_added.size());
  }

 protected:
  // Quits the RunLoop on Mojo request flow completion.
  base::OnceClosure quit_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  base::test::ScopedFeatureList feature_list_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)

 private:
#if BUILDFLAG(IS_WIN)
  std::optional<spellcheck::ScopedDisableBrowserSpellCheckerForTesting>
      disable_browser_spell_checker_;
#endif

  // Mocked RenderProcessHost.
  std::unique_ptr<content::MockRenderProcessHost> renderer_;

  // Not owned preferences service.
  raw_ptr<PrefService> prefs_;

  // Binding to receive the SpellChecker request flow.
  mojo::Receiver<spellcheck::mojom::SpellChecker> receiver_{this};

  // Used to verify the SpellChecker request flow.
  bool bound_connection_closed_;
  bool custom_dictionary_changed_called_;
  bool initialize_spellcheck_called_;
  bool spellcheck_enabled_state_;
};

class SpellcheckServiceHostBrowserTest : public SpellcheckServiceBrowserTest {
 public:
  SpellcheckServiceHostBrowserTest() = default;

  SpellcheckServiceHostBrowserTest(const SpellcheckServiceHostBrowserTest&) =
      delete;
  SpellcheckServiceHostBrowserTest& operator=(
      const SpellcheckServiceHostBrowserTest&) = delete;

  void RequestDictionary() {
    mojo::Remote<spellcheck::mojom::SpellCheckInitializationHost> interface;
    RequestSpellCheckInitializationHost(&interface);

    interface->RequestDictionary();
  }

  void NotifyChecked() {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> interface;
    RequestSpellCheckHost(&interface);

    const bool misspelt = true;
    base::UTF8ToUTF16("hallo", 5, &word_);
    interface->NotifyChecked(word_, misspelt);
    base::RunLoop().RunUntilIdle();
  }

  void CallSpellingService() {
    mojo::Remote<spellcheck::mojom::SpellCheckHost> interface;
    RequestSpellCheckHost(&interface);

    base::UTF8ToUTF16("hello", 5, &word_);
    interface->CallSpellingService(
        word_,
        base::BindOnce(&SpellcheckServiceHostBrowserTest::SpellingServiceDone,
                       base::Unretained(this)));

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();

    EXPECT_TRUE(spelling_service_done_called_);
  }

 private:
  void RequestSpellCheckHost(
      mojo::Remote<spellcheck::mojom::SpellCheckHost>* interface) {
    SpellCheckHostChromeImpl::Create(GetRenderer()->GetID(),
                                     interface->BindNewPipeAndPassReceiver());
  }

  void RequestSpellCheckInitializationHost(
      mojo::Remote<spellcheck::mojom::SpellCheckInitializationHost>*
          interface) {
    SpellCheckInitializationHostImpl::Create(
        GetRenderer()->GetID(), interface->BindNewPipeAndPassReceiver());
  }

  void SpellingServiceDone(bool success,
                           const std::vector<::SpellCheckResult>& results) {
    spelling_service_done_called_ = true;
    if (quit_)
      std::move(quit_).Run();
  }

  bool spelling_service_done_called_ = false;
  std::u16string word_;
};

// Disable spell check should disable spelling service
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       DisableSpellcheckDisableSpellingService) {
  InitSpellcheck(true, "", "en-US");
  GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService,
                         true);

  EnableSpellcheck(false);
  EXPECT_FALSE(
      GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckUseSpellingService));
}

#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       DisableSpellcheckIfDictionaryIsEmpty) {
  InitSpellcheck(true, "", "en-US");
  SetMultiLingualDictionaries("");

  EXPECT_FALSE(GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
}
#endif  // !BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Removing a spellcheck language from accept languages should not remove it
// from spellcheck languages list on CrOS.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       RemoveSpellcheckLanguageFromAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,es,ru");
  EXPECT_EQ("en-US,fr", GetMultilingualDictionaries());
}
#else
// Removing a spellcheck language from accept languages should remove it from
// spellcheck languages list as well.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       RemoveSpellcheckLanguageFromAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,es,ru");
  EXPECT_EQ("en-US", GetMultilingualDictionaries());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Keeping spellcheck languages in accept languages should not alter spellcheck
// languages list.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       KeepSpellcheckLanguagesInAcceptLanguages) {
  InitSpellcheck(true, "", "en-US,fr");
  SetAcceptLanguages("en-US,fr,es");
  EXPECT_EQ("en-US,fr", GetMultilingualDictionaries());
}

// Starting with spellcheck enabled should send the 'enable spellcheck' message
// to the renderer. Consequently disabling spellcheck should send the 'disable
// spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithSpellcheck) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with only a single-language spellcheck setting should send the
// 'enable spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithSingularLanguagePreference) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with a multi-language spellcheck setting should send the 'enable
// spellcheck' message to the renderer. Consequently removing spellcheck
// languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithMultiLanguagePreference) {
  InitSpellcheck(true, "", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting with both single-language and multi-language spellcheck settings
// should send the 'enable spellcheck' message to the renderer. Consequently
// removing spellcheck languages should disable spellcheck.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       StartWithBothLanguagePreferences) {
  InitSpellcheck(true, "en-US", "en-US,fr");
  EXPECT_TRUE(GetEnableSpellcheckState());

  SetMultiLingualDictionaries("");
  EXPECT_FALSE(GetEnableSpellcheckState(true));
}

// Starting without spellcheck languages should send the 'disable spellcheck'
// message to the renderer. Consequently adding spellchecking languages should
// enable spellcheck.
// Flaky, see https://crbug.com/600153
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       DISABLED_StartWithoutLanguages) {
  InitSpellcheck(true, "", "");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  SetMultiLingualDictionaries("en-US");
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// Starting with spellcheck disabled should send the 'disable spellcheck'
// message to the renderer. Consequently enabling spellcheck should send the
// 'enable spellcheck' message to the renderer.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, StartWithoutSpellcheck) {
  InitSpellcheck(false, "", "en-US,fr");
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  EnableSpellcheck(true);
  EXPECT_TRUE(GetEnableSpellcheckState());
}

// A custom dictionary state change should send a 'custom dictionary changed'
// message to the renderer, regardless of the spellcheck enabled state.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, CustomDictionaryChanged) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());

  EnableSpellcheck(false);
  EXPECT_FALSE(GetEnableSpellcheckState(true));

  ChangeCustomDictionary();
  EXPECT_TRUE(GetCustomDictionaryChangedState());
}

// Regression test for https://crbug.com/854540.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       CustomDictionaryChangedAfterRendererCrash) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  // Kill the renderer process.
  content::RenderProcessHost* process = browser()
                                            ->tab_strip_model()
                                            ->GetActiveWebContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetProcess();
  content::RenderProcessHostWatcher crash_observer(
      process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(process->Shutdown(0));
  crash_observer.Wait();

  // Change the custom dictionary - the test passes if there were no crashes or
  // hangs.
  ChangeCustomDictionary();
}

// Starting with only a single-language spellcheck setting, the host should
// initialize the renderer's spellcheck system, and the same if the renderer
// explicity requests the spellcheck dictionaries.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceHostBrowserTest, RequestDictionary) {
  InitSpellcheck(true, "en-US", "");
  EXPECT_TRUE(GetEnableSpellcheckState());

  RequestDictionary();
  EXPECT_TRUE(GetEnableSpellcheckState());
}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
// When the renderer requests the spelling service for correcting text, the
// render process host should call the remote spelling service.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceHostBrowserTest, CallSpellingService) {
  CallSpellingService();
}
#endif  // BUILDFLAG(USE_RENDERER_SPELLCHECKER)

// Tests that we can delete a corrupted BDICT file used by hunspell. We do not
// run this test on Mac because Mac does not use hunspell by default.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, DeleteCorruptedBDICT) {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  if (spellcheck::UseBrowserSpellChecker()) {
    // If doing native spell checking on Windows, Hunspell dictionaries are not
    // used for en-US, so the corrupt dictionary event will never be raised.
    // Skip this test.
    return;
  }
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

  // Corrupted BDICT data: please do not use this BDICT data for other tests.
  const uint8_t kCorruptedBDICT[] = {
      0x42, 0x44, 0x69, 0x63, 0x02, 0x00, 0x01, 0x00, 0x20, 0x00, 0x00, 0x00,
      0x3b, 0x00, 0x00, 0x00, 0x65, 0x72, 0xe0, 0xac, 0x27, 0xc7, 0xda, 0x66,
      0x6d, 0x1e, 0xa6, 0x35, 0xd1, 0xf6, 0xb7, 0x35, 0x32, 0x00, 0x00, 0x00,
      0x38, 0x00, 0x00, 0x00, 0x39, 0x00, 0x00, 0x00, 0x3a, 0x00, 0x00, 0x00,
      0x0a, 0x0a, 0x41, 0x46, 0x20, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe6,
      0x49, 0x00, 0x68, 0x02, 0x73, 0x06, 0x74, 0x0b, 0x77, 0x11, 0x79, 0x15,
  };

  // Write the corrupted BDICT data to create a corrupted BDICT file.
  base::FilePath dict_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_APP_DICTIONARIES, &dict_dir));
  base::FilePath bdict_path =
      spellcheck::GetVersionedFileName("en-US", dict_dir);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    bool success = base::WriteFile(
        bdict_path, base::as_bytes(base::make_span(kCorruptedBDICT)));
    EXPECT_TRUE(success);
  }

  // Attach an event to the SpellcheckService object so we can receive its
  // status updates.
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  SpellcheckService::AttachStatusEvent(&event);

  BrowserContext* context = GetContext();

  // Ensure that the SpellcheckService object does not already exist. Otherwise
  // the next line will not force creation of the SpellcheckService and the
  // test will fail.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context,
          false));
  ASSERT_EQ(nullptr, service);

  // Getting the spellcheck_service will initialize the SpellcheckService
  // object with the corrupted BDICT file created above since the hunspell
  // dictionary is loaded in the SpellcheckService constructor right now.
  // The SpellCheckHost object will send a BDICT_CORRUPTED event.
  service = SpellcheckServiceFactory::GetForContext(context);
  ASSERT_NE(nullptr, service);
  ASSERT_TRUE(service->dictionaries_loaded());

  // Check the received event. Also we check if Chrome has successfully deleted
  // the corrupted dictionary. We delete the corrupted dictionary to avoid
  // leaking it when this test fails.
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(SpellcheckService::BDICT_CORRUPTED,
            SpellcheckService::GetStatusEvent());
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (base::PathExists(bdict_path)) {
    ADD_FAILURE();
    EXPECT_TRUE(base::DeletePathRecursively(bdict_path));
  }
}

// Checks that preferences migrate correctly.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesMigrated) {
  base::Value::List empty_list;
  GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                      std::move(empty_list));
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "en-US");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have been migrated.
  ASSERT_EQ(
      1u,
      GetPrefs()->GetList(spellcheck::prefs::kSpellCheckDictionaries).size());
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                  .is_string());
  EXPECT_EQ("en-US",
            GetPrefs()
                ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                .GetString());
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that preferences are not migrated when they shouldn't be.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest, PreferencesNotMigrated) {
  base::Value::List dictionaries;
  dictionaries.Append("en-US");
  GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                      std::move(dictionaries));
  GetPrefs()->SetString(spellcheck::prefs::kSpellCheckDictionary, "fr");

  // Create a SpellcheckService which will migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  // Make sure the preferences have not been migrated.
  ASSERT_EQ(
      1u,
      GetPrefs()->GetList(spellcheck::prefs::kSpellCheckDictionaries).size());
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                  .is_string());
  EXPECT_EQ("en-US",
            GetPrefs()
                ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                .GetString());
  EXPECT_TRUE(
      GetPrefs()->GetString(spellcheck::prefs::kSpellCheckDictionary).empty());
}

// Checks that, if a user has spellchecking disabled, nothing changes
// during migration.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       SpellcheckingDisabledPreferenceMigration) {
  base::Value::List dictionaries;
  dictionaries.Append("en-US");
  GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                      std::move(dictionaries));
  GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);

  // Migrate the preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_FALSE(GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
  EXPECT_EQ(
      1U,
      GetPrefs()->GetList(spellcheck::prefs::kSpellCheckDictionaries).size());
}

// Make sure preferences get preserved and spellchecking stays enabled.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceBrowserTest,
                       MultilingualPreferenceNotMigrated) {
  base::Value::List dictionaries;
  dictionaries.Append("en-US");
  dictionaries.Append("fr");
  GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                      std::move(dictionaries));
  GetPrefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);

  // Should not migrate any preferences.
  SpellcheckServiceFactory::GetForContext(GetContext());

  EXPECT_TRUE(GetPrefs()->GetBoolean(spellcheck::prefs::kSpellCheckEnable));
  EXPECT_EQ(
      2U,
      GetPrefs()->GetList(spellcheck::prefs::kSpellCheckDictionaries).size());
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                  .is_string());
  EXPECT_EQ("en-US",
            GetPrefs()
                ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[0]
                .GetString());
  ASSERT_TRUE(GetPrefs()
                  ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[1]
                  .is_string());
  EXPECT_EQ("fr", GetPrefs()
                      ->GetList(spellcheck::prefs::kSpellCheckDictionaries)[1]
                      .GetString());
}

#if BUILDFLAG(IS_WIN)
class SpellcheckServiceWindowsHybridBrowserTest
    : public SpellcheckServiceBrowserTest {
 public:
  SpellcheckServiceWindowsHybridBrowserTest()
      : SpellcheckServiceBrowserTest(/* use_browser_spell_checker=*/true) {}
};

IN_PROC_BROWSER_TEST_F(SpellcheckServiceWindowsHybridBrowserTest,
                       WindowsHybridSpellcheck) {
  // This test specifically covers the case where spellcheck delayed
  // initialization is not enabled, so return early if it is. Other tests
  // cover the case where delayed initialization is enabled.
  if (base::FeatureList::IsEnabled(spellcheck::kWinDelaySpellcheckServiceInit))
    return;

  ASSERT_TRUE(spellcheck::UseBrowserSpellChecker());

  // Note that the base class forces dictionary sync to not be performed, which
  // on its own would have created a SpellcheckService object. So testing here
  // that we are still instantiating the SpellcheckService as a browser startup
  // task to support hybrid spellchecking.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          GetContext(), /* create */ false));
  ASSERT_NE(nullptr, service);

  // The list of Windows spellcheck languages should have been populated by at
  // least one language. This assures that the spellcheck context menu will
  // include Windows spellcheck languages that lack Hunspell support.
  EXPECT_TRUE(service->dictionaries_loaded());
  EXPECT_FALSE(service->windows_spellcheck_dictionary_map_.empty());
}

class SpellcheckServiceWindowsHybridBrowserTestDelayInit
    : public SpellcheckServiceBrowserTest {
 public:
  SpellcheckServiceWindowsHybridBrowserTestDelayInit()
      : SpellcheckServiceBrowserTest(/* use_browser_spell_checker=*/true) {}

  void SetUp() override {
    // Don't initialize the SpellcheckService on browser launch.
    feature_list_.InitAndEnableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);

    // Add command line switch that forces first run state, to test whether
    // primary preferred language has its spellcheck dictionary enabled by
    // default for non-Hunspell languages.
    first_run::ResetCachedSentinelDataForTesting();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceFirstRun);

    InProcessBrowserTest::SetUp();
  }

  void OnDictionariesInitialized() {
    dictionaries_initialized_received_ = true;
    if (quit_on_callback_)
      std::move(quit_on_callback_).Run();
  }

 protected:
  void RunUntilCallbackReceived() {
    if (dictionaries_initialized_received_)
      return;
    base::RunLoop run_loop;
    quit_on_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    // reset status.
    dictionaries_initialized_received_ = false;
  }

 private:
  bool dictionaries_initialized_received_ = false;

  // Quits the RunLoop on receiving the callback from InitializeDictionaries.
  base::OnceClosure quit_on_callback_;
};

// Used for faking the presence of Windows spellcheck dictionaries.
const std::vector<std::string> kWindowsSpellcheckLanguages = {
    "fi-FI",  // Finnish has no Hunspell support.
    "fr-FR",  // French has both Windows and Hunspell support.
    "pt-BR"   // Portuguese (Brazil) has both Windows and Hunspell support, but
              // generic pt does not have Hunspell support.
};

// Used for testing whether primary preferred language is enabled by default for
// spellchecking.
const char kAcceptLanguages[] = "fi-FI,fi,ar-AR,fr-FR,fr,hr,ceb,pt-BR,pt";
const std::vector<std::string> kSpellcheckDictionariesBefore = {
    // Note that Finnish is initially unset, but has Windows spellcheck
    // dictionary present.
    "ar",     // Arabic has no Hunspell support, and its Windows spellcheck
              // dictionary is not present.
    "fr-FR",  // French has both Windows and Hunspell support, and its Windows
              // spellcheck dictionary is present.
    "fr",     // Generic language should also be toggleable for spellcheck.
    "hr",     // Croatian has Hunspell support.
    "ceb",    // Cebuano doesn't have any dictionary support and should be
              // removed from preferences.
    "pt-BR",  // Portuguese (Brazil) has both Windows and Hunspell support, and
              // its Windows spellcheck dictionary is present.
    "pt"      // Generic language should also be toggleable for spellcheck.
};

const std::vector<std::string> kSpellcheckDictionariesAfter = {
    "fi",     // Finnish should have been enabled for spellchecking since
              // it's the primary language.
    "fr-FR",  // French should still be there.
    "fr",     // Should still be entry for generic French.
    "hr",     // So should Croatian.
    "pt-BR",  // Portuguese (Brazil) should still be there.
    "pt"      // Should still be entry for generic Portuguese.
};

// As a prelude to the next test, sets the initial accept languages and
// spellcheck language preferences for the test profile.
IN_PROC_BROWSER_TEST_F(SpellcheckServiceWindowsHybridBrowserTestDelayInit,
                       PRE_WindowsHybridSpellcheckDelayInit) {
  GetPrefs()->SetString(language::prefs::kSelectedLanguages, kAcceptLanguages);
  base::Value::List spellcheck_dictionaries_list;
  for (const auto& dictionary : kSpellcheckDictionariesBefore) {
    spellcheck_dictionaries_list.Append(std::move(dictionary));
  }
  GetPrefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                      std::move(spellcheck_dictionaries_list));
}

IN_PROC_BROWSER_TEST_F(SpellcheckServiceWindowsHybridBrowserTestDelayInit,
                       WindowsHybridSpellcheckDelayInit) {
  ASSERT_TRUE(spellcheck::UseBrowserSpellChecker());

  // Note that the base class forces dictionary sync to not be performed, and
  // the kWinDelaySpellcheckServiceInit flag is set, which together should
  // prevent creation of a SpellcheckService object on browser startup. So
  // testing here that this is indeed the case.
  SpellcheckService* service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          GetContext(), /* create */ false));
  EXPECT_EQ(nullptr, service);

  // Now create the SpellcheckService but don't call InitializeDictionaries().
  service = static_cast<SpellcheckService*>(
      SpellcheckServiceFactory::GetInstance()->GetServiceForBrowserContext(
          GetContext(), /* create */ true));

  ASSERT_NE(nullptr, service);

  // The list of Windows spellcheck languages should not have been populated
  // yet since InitializeDictionaries() has not been called.
  EXPECT_FALSE(service->dictionaries_loaded());
  EXPECT_TRUE(service->windows_spellcheck_dictionary_map_.empty());

  // Fake the presence of Windows spellcheck dictionaries.
  service->AddSpellcheckLanguagesForTesting(kWindowsSpellcheckLanguages);

  service->InitializeDictionaries(
      base::BindOnce(&SpellcheckServiceWindowsHybridBrowserTestDelayInit::
                         OnDictionariesInitialized,
                     base::Unretained(this)));

  RunUntilCallbackReceived();
  EXPECT_TRUE(service->dictionaries_loaded());
  // The list of Windows spellcheck languages should now have been populated.
  std::map<std::string, std::string>
      windows_spellcheck_dictionary_map_first_call =
          service->windows_spellcheck_dictionary_map_;
  EXPECT_FALSE(windows_spellcheck_dictionary_map_first_call.empty());

  // Check that the primary accept language has spellchecking enabled and
  // that languages with no spellcheck support have spellchecking disabled.
  EXPECT_EQ(kAcceptLanguages,
            GetPrefs()->GetString(language::prefs::kAcceptLanguages));
  const base::Value::List& dictionaries_list =
      GetPrefs()->GetList(spellcheck::prefs::kSpellCheckDictionaries);
  std::vector<std::string> actual_dictionaries;
  for (const auto& dictionary : dictionaries_list) {
    actual_dictionaries.push_back(dictionary.GetString());
  }
  EXPECT_EQ(kSpellcheckDictionariesAfter, actual_dictionaries);

  // It should be safe to call InitializeDictionaries again (it should
  // immediately run the callback).
  service->InitializeDictionaries(
      base::BindOnce(&SpellcheckServiceWindowsHybridBrowserTestDelayInit::
                         OnDictionariesInitialized,
                     base::Unretained(this)));

  RunUntilCallbackReceived();
  EXPECT_TRUE(service->dictionaries_loaded());
  EXPECT_EQ(windows_spellcheck_dictionary_map_first_call,
            service->windows_spellcheck_dictionary_map_);
}
#endif  // BUILDFLAG(IS_WIN)
