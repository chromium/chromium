// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

// spellcheck_per_process_browsertest.cc

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_isolation/chrome_site_per_process_test.h"
#include "chrome/browser/spellchecker/spell_check_host_chrome_impl.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/spellcheck/spellcheck_buildflags.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
#include "chrome/browser/spellchecker/test/spellcheck_panel_browsertest_helper.h"
#include "components/spellcheck/common/spellcheck_panel.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)

// Class to sniff incoming spellcheck Mojo SpellCheckHost messages.
class MockSpellCheckHost : spellcheck::mojom::SpellCheckHost {
 public:
  explicit MockSpellCheckHost(content::RenderProcessHost* process_host)
      : process_host_(process_host) {}

  MockSpellCheckHost(const MockSpellCheckHost&) = delete;
  MockSpellCheckHost& operator=(const MockSpellCheckHost&) = delete;

  ~MockSpellCheckHost() override {}

  content::RenderProcessHost* process_host() const { return process_host_; }

  const std::u16string& text() const { return text_; }

  bool HasReceivedText() const { return text_received_; }

  void Wait() {
    if (text_received_)
      return;

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitUntilTimeout() {
    if (text_received_)
      return;

    auto ui_task_runner = content::GetUIThreadTaskRunner({});
    ui_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockSpellCheckHost::Timeout, base::Unretained(this)),
        base::Seconds(1));

    base::RunLoop run_loop;
    quit_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SpellCheckHostReceiver(
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
    EXPECT_FALSE(receiver_.is_bound());
    receiver_.Bind(std::move(receiver));
  }

 private:
  void TextReceived(const std::u16string& text) {
    text_received_ = true;
    text_ = text;
    receiver_.reset();
    if (quit_)
      std::move(quit_).Run();
  }

  void Timeout() {
    if (quit_)
      std::move(quit_).Run();
  }

  // spellcheck::mojom::SpellCheckHost:
  void NotifyChecked(const std::u16string& word, bool misspelled) override {}

#if BUILDFLAG(USE_RENDERER_SPELLCHECKER)
  void CallSpellingService(const std::u16string& text,
                           CallSpellingServiceCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::move(callback).Run(true, std::vector<SpellCheckResult>());
    TextReceived(text);
  }
#endif

#if BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  void RequestTextCheck(const std::u16string& text,
                        RequestTextCheckCallback callback) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    std::move(callback).Run(std::vector<SpellCheckResult>());
    TextReceived(text);
  }

  void CheckSpelling(const std::u16string& word,
                     CheckSpellingCallback) override {}
  void FillSuggestionList(const std::u16string& word,
                          FillSuggestionListCallback) override {}

#if BUILDFLAG(IS_WIN)
  void InitializeDictionaries(
      InitializeDictionariesCallback callback) override {
    if (base::FeatureList::IsEnabled(
            spellcheck::kWinDelaySpellcheckServiceInit)) {
      SpellcheckService* spellcheck = SpellcheckServiceFactory::GetForContext(
          process_host()->GetBrowserContext());

      if (!spellcheck) {  // Teardown.
        std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                                /*enable=*/false);
        return;
      }

      dictionaries_loaded_callback_ = std::move(callback);

      spellcheck->InitializeDictionaries(
          base::BindOnce(&MockSpellCheckHost::OnDictionariesInitialized,
                         base::Unretained(this)));
      return;
    }

    NOTREACHED_IN_MIGRATION();
    std::move(callback).Run(/*dictionaries=*/{}, /*custom_words=*/{},
                            /*enable=*/false);
  }

  void OnDictionariesInitialized() {
    if (dictionaries_loaded_callback_) {
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries;
      dictionaries.push_back(spellcheck::mojom::SpellCheckBDictLanguage::New(
          base::File(), "en-US"));

      std::move(dictionaries_loaded_callback_)
          .Run(std::move(dictionaries), {}, true);
    }
  }

  // Callback passed as argument to InitializeDictionaries, and invoked when
  // the dictionaries are loaded for the first time.
  InitializeDictionariesCallback dictionaries_loaded_callback_;
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(USE_BROWSER_SPELLCHECKER)

#if BUILDFLAG(IS_ANDROID)
  // spellcheck::mojom::SpellCheckHost:
  void DisconnectSessionBridge() override {}
#endif

  raw_ptr<content::RenderProcessHost> process_host_;
  bool text_received_ = false;
  std::u16string text_;
  mojo::Receiver<spellcheck::mojom::SpellCheckHost> receiver_{this};
  base::OnceClosure quit_;
};

class SpellCheckBrowserTestHelper {
 public:
  SpellCheckBrowserTestHelper() {
    SpellCheckHostChromeImpl::OverrideBinderForTesting(
        base::BindRepeating(&SpellCheckBrowserTestHelper::BindSpellCheckHost,
                            base::Unretained(this)));
  }

  SpellCheckBrowserTestHelper(const SpellCheckBrowserTestHelper&) = delete;
  SpellCheckBrowserTestHelper& operator=(const SpellCheckBrowserTestHelper&) =
      delete;

  ~SpellCheckBrowserTestHelper() {
    SpellCheckHostChromeImpl::OverrideBinderForTesting(base::NullCallback());
  }

  // Retrieves the registered MockSpellCheckHost for the given
  // RenderProcessHost. It will return nullptr if the RenderProcessHost was
  // initialized while a different instance of ContentBrowserClient was in
  // action.
  MockSpellCheckHost* GetSpellCheckHostForProcess(
      content::RenderProcessHost* process_host) const {
    for (auto& spell_check_host : spell_check_hosts_) {
      if (spell_check_host->process_host() == process_host)
        return spell_check_host.get();
    }
    return nullptr;
  }

  void RunUntilBind() {
    if (!spell_check_hosts_.empty())
      return;

    base::RunLoop run_loop;
    quit_on_bind_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void RunUntilBindOrTimeout() {
    if (!spell_check_hosts_.empty())
      return;

    auto ui_task_runner = content::GetUIThreadTaskRunner({});
    ui_task_runner->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SpellCheckBrowserTestHelper::Timeout,
                       base::Unretained(this)),
        base::Seconds(1));

    base::RunLoop run_loop;
    quit_on_bind_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void BindSpellCheckHost(
      int render_process_id,
      mojo::PendingReceiver<spellcheck::mojom::SpellCheckHost> receiver) {
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(render_process_id);
    auto spell_check_host = std::make_unique<MockSpellCheckHost>(host);
    spell_check_host->SpellCheckHostReceiver(std::move(receiver));
    spell_check_hosts_.push_back(std::move(spell_check_host));
    if (quit_on_bind_closure_)
      std::move(quit_on_bind_closure_).Run();
  }

  void Timeout() {
    if (quit_on_bind_closure_)
      std::move(quit_on_bind_closure_).Run();
  }

  base::OnceClosure quit_on_bind_closure_;
  std::vector<std::unique_ptr<MockSpellCheckHost>> spell_check_hosts_;
};

class ChromeSitePerProcessSpellCheckTest : public ChromeSitePerProcessTest {
 public:
  ChromeSitePerProcessSpellCheckTest() = default;

  void SetUp() override {
#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
    // When delayed initialization of the spellcheck service is enabled by
    // default, want to maintain test coverage for the older code path that
    // initializes spellcheck on browser startup.
    feature_list_.InitAndDisableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)

    ChromeSitePerProcessTest::SetUp();
  }

 protected:
  // Tests that spelling in out-of-process subframes is checked.
  // See crbug.com/638361 for details.
  void RunOOPIFSpellCheckTest() {
    SpellCheckBrowserTestHelper spell_check_helper;

    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
    spell_check_helper.RunUntilBind();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* cross_site_subframe =
        ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

    MockSpellCheckHost* spell_check_host =
        spell_check_helper.GetSpellCheckHostForProcess(
            cross_site_subframe->GetProcess());
    spell_check_host->Wait();

    EXPECT_EQ(u"zz.", spell_check_host->text());
  }

  // Tests that after disabling spellchecking, spelling in new out-of-process
  // subframes is not checked. See crbug.com/789273 for details.
  // https://crbug.com/944428
  void RunOOPIFDisabledSpellCheckTest() {
    SpellCheckBrowserTestHelper spell_check_helper;

    content::BrowserContext* browser_context =
        static_cast<content::BrowserContext*>(browser()->profile());

    // Initiate a SpellcheckService
    SpellcheckServiceFactory::GetForContext(browser_context);

    // Disable spellcheck
    PrefService* prefs = user_prefs::UserPrefs::Get(browser_context);
    prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);
    base::RunLoop().RunUntilIdle();

    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
    spell_check_helper.RunUntilBindOrTimeout();

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* cross_site_subframe =
        ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

    MockSpellCheckHost* spell_check_host =
        spell_check_helper.GetSpellCheckHostForProcess(
            cross_site_subframe->GetProcess());

    // The renderer makes no SpellCheckHostReceiver at all, in which case no
    // SpellCheckHost is bound and no spellchecking will be done.
    EXPECT_FALSE(spell_check_host);

    prefs->SetBoolean(spellcheck::prefs::kSpellCheckEnable, true);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessSpellCheckTest,
                       OOPIFSpellCheckTest) {
  RunOOPIFSpellCheckTest();
}

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessSpellCheckTest,
                       OOPIFDisabledSpellCheckTest) {
  RunOOPIFDisabledSpellCheckTest();
}

#if BUILDFLAG(HAS_SPELLCHECK_PANEL)
// Tests that the OSX spell check panel can be opened from an out-of-process
// subframe, crbug.com/712395
IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessSpellCheckTest,
                       OOPIFSpellCheckPanelTest) {
  spellcheck::SpellCheckPanelBrowserTestHelper test_helper;

  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_contenteditable_in_cross_site_subframe.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* cross_site_subframe =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);

  EXPECT_TRUE(cross_site_subframe->IsCrossProcessSubframe());

  mojo::Remote<spellcheck::mojom::SpellCheckPanel> spell_check_panel_client;
  cross_site_subframe->GetRemoteInterfaces()->GetInterface(
      spell_check_panel_client.BindNewPipeAndPassReceiver());
  spell_check_panel_client->ToggleSpellPanel(false);
  test_helper.RunUntilBind();

  spellcheck::SpellCheckMockPanelHost* host =
      test_helper.GetSpellCheckMockPanelHostForProcess(
          cross_site_subframe->GetProcess());
  EXPECT_TRUE(host->SpellingPanelVisible());
}
#endif  // BUILDFLAG(HAS_SPELLCHECK_PANEL)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
class ChromeSitePerProcessSpellCheckTestDelayInit
    : public ChromeSitePerProcessSpellCheckTest {
 public:
  ChromeSitePerProcessSpellCheckTestDelayInit() = default;

  void SetUp() override {
    // Don't initialize the SpellcheckService on browser launch.
    feature_list_.InitAndEnableFeature(
        spellcheck::kWinDelaySpellcheckServiceInit);

    ChromeSitePerProcessTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessSpellCheckTestDelayInit,
                       OOPIFSpellCheckTest) {
  RunOOPIFSpellCheckTest();
}

IN_PROC_BROWSER_TEST_F(ChromeSitePerProcessSpellCheckTestDelayInit,
                       OOPIFDisabledSpellCheckTest) {
  RunOOPIFDisabledSpellCheckTest();
}
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(USE_BROWSER_SPELLCHECKER)
