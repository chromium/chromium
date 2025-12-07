// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/password_manager/password_manager_test_base.h"
#include "chrome/browser/password_manager/passwords_navigation_observer.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"

namespace {

using password_manager::MatchesFormExceptStore;
using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::SizeIs;

class CredentialManagerBrowserTest : public PasswordManagerBrowserTestBase {
 public:
  CredentialManagerBrowserTest() = default;

  CredentialManagerBrowserTest(const CredentialManagerBrowserTest&) = delete;
  CredentialManagerBrowserTest& operator=(const CredentialManagerBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    PasswordManagerBrowserTestBase::SetUpOnMainThread();
    // Redirect all requests to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool IsShowingAccountChooser() {
    return PasswordsModelDelegateFromWebContents(WebContents())->GetState() ==
           password_manager::ui::CREDENTIAL_REQUEST_STATE;
  }

  // Similarly to PasswordManagerBrowserTestBase::NavigateToFile this is a
  // wrapper around ui_test_utils::NavigateURL that waits until DidFinishLoad()
  // fires. Different to NavigateToFile this method allows passing a test_server
  // and modifications to the hostname.
  void NavigateToURL(const net::EmbeddedTestServer& test_server,
                     const std::string& hostname,
                     const std::string& relative_url) {
    PasswordsNavigationObserver observer(WebContents());
    GURL url = test_server.GetURL(hostname, relative_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_TRUE(observer.Wait());
  }

  // Triggers a call to `navigator.credentials.get` to retrieve passwords, waits
  // for success, and ASSERTs that |expect_has_results| is satisfied.
  void TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
      content::WebContents* web_contents,
      bool expect_has_results) {
    ASSERT_EQ(
        expect_has_results,
        content::EvalJs(
            web_contents,
            "navigator.credentials.get({password: true}).then(c => !!c);"));
  }

  // Schedules a call to be made to navigator.credentials.store() in the
  // `unload` handler to save a credential with |username| and |password|.
  void ScheduleNavigatorStoreCredentialAtUnload(
      content::WebContents* web_contents,
      const char* username,
      const char* password) {
    ASSERT_TRUE(content::ExecJs(
        web_contents,
        base::StringPrintf(
            "window.addEventListener(\"unload\", () => {"
            "  var c = new PasswordCredential({ id: '%s', password: '%s' });"
            "  navigator.credentials.store(c);"
            "});",
            username, password)));
  }

  // Tests that when navigator.credentials.store() is called in an `unload`
  // handler before a same-RenderFrame navigation, the request is either dropped
  // or serviced in the context of the old document.
  //
  // If |preestablish_mojo_pipe| is set, then the CredentialManagerClient will
  // establish the Mojo connection to the ContentCredentialManager ahead of
  // time, instead of letting the Mojo connection be established on-demand when
  // the call to store() triggered from the unload handler.
  void TestStoreInUnloadHandlerForSameSiteNavigation(
      bool preestablish_mojo_pipe) {
    WebContents()->GetController().GetBackForwardCache().DisableForTesting(
        content::BackForwardCache::TEST_USES_UNLOAD_EVENT);

    // Use URLs that differ on subdomains so we can tell which one was used for
    // saving, but they still belong to the same SiteInstance, so they will be
    // renderered in the same RenderFrame (in the same process).
    const GURL a_url1 = https_test_server().GetURL("foo.a.com", "/title1.html");
    const GURL a_url2 = https_test_server().GetURL("bar.a.com", "/title2.html");

    // Navigate to a mostly empty page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url1));

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());

    EXPECT_FALSE(client->has_binding_for_credential_manager());
    if (preestablish_mojo_pipe) {
      ASSERT_NO_FATAL_FAILURE(
          TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
              WebContents(), false));
      EXPECT_TRUE(client->has_binding_for_credential_manager());
    }

    // Schedule storing a credential on the `unload` event.
    ASSERT_NO_FATAL_FAILURE(ScheduleNavigatorStoreCredentialAtUnload(
        WebContents(), "user", "hunter2"));

    // Trigger a same-site navigation.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url2));

    // Ensure that the old document no longer has a mojom::CredentialManager
    // interface connection to the ContentCredentialManager, nor can it get one
    // later.
    //
    // The sequence of events for same-RFH navigations is as follows:
    //  1.) FrameHostMsg_DidStartProvisionalLoad
    //  ... waiting for first response byte ...
    //  2.) FrameLoader::PrepareForCommit
    //  2.1) Document::Shutdown (old Document)
    //  3.) mojom::FrameHost::DidCommitProvisionalLoad (new load)
    //  ... loading ...
    //  4.) FrameHostMsg_DidStopLoading
    //  5.) content::WaitForLoadStop inside NavigateToURL returns
    //  6.) NavigateToURL returns
    //
    // After Step 2.1, the old Document no longer executes any author JS, so
    // there can be no more calls to the Credential Management API, hence no
    // more InterfaceRequests for mojom::CredentialManager.
    //
    // Because the InterfaceRegistry, through which the client end of the
    // mojom::CredentialManager interface to the ContentCredentialManager is
    // retrieved, is re-bound by the RenderFrameHostImpl to a new pipe on
    // DidCommitProvisionalLoad, any InterfaceRequest messages issued before or
    // during Step 2.1 will either have already been dispatched on the browser
    // side and serviced before DidCommitProvisionalLoad in Step 3, or will be
    // ignored altogether.
    //
    // Hence it is sufficient to check that the Mojo connection is closed now
    // after NavigateToURL above has returned.
    EXPECT_FALSE(client->has_binding_for_credential_manager());

    // Ensure that the navigator.credentials.store() call issued on the previous
    // mojom::CredentialManager connection was either serviced in the context of
    // the old URL, |a_url|, or dropped altogether.
    //
    // The behavior is non-deterministic because the mojom::CredentialManager
    // interface is not Channel-associated, so message ordering with legacy IPC
    // messages is not preserved.
    //
    // If the store() method invoked from the `unload` handler (in Step 2.1)
    // happens to be speedily dispatched before DidCommitProvisionalLoad, it
    // will have been serviced in the context of the old document. Otherwise the
    // ContentCredentialManager should have closed the underlying interface
    // connection in response to DidCommitProvisionalLoad in Step 3, and the
    // method call should be ignored.
    if (!client->was_store_ever_called()) {
      return;
    }

    BubbleObserver prompt_observer(WebContents());
    prompt_observer.WaitForAutomaticSavePrompt();
    ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
    prompt_observer.AcceptSavePrompt();

    WaitForPasswordStore();

    password_manager::TestPasswordStore* test_password_store =
        static_cast<password_manager::TestPasswordStore*>(
            ProfilePasswordStoreFactory::GetForProfile(
                browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
                .get());

    ASSERT_EQ(1u, test_password_store->stored_passwords().size());
    password_manager::PasswordForm signin_form =
        test_password_store->stored_passwords().begin()->second[0];
    EXPECT_EQ(u"user", signin_form.username_value);
    EXPECT_EQ(u"hunter2", signin_form.password_value);
    EXPECT_EQ(a_url1.DeprecatedGetOriginAsURL().spec(),
              signin_form.signon_realm);
    EXPECT_EQ(a_url1.DeprecatedGetOriginAsURL(), signin_form.url);
  }

  // Tests the when navigator.credentials.store() is called in an `unload`
  // handler before a cross-site transfer navigation, the request is ignored.
  //
  // If |preestablish_mojo_pipe| is set, then the CredentialManagerClient will
  // establish the Mojo connection to the ContentCredentialManager ahead of
  // time, instead of letting the Mojo connection be established on-demand when
  // the call to store() triggered from the unload handler.
  void TestStoreInUnloadHandlerForCrossSiteNavigation(
      bool preestablish_mojo_pipe) {
    WebContents()->GetController().GetBackForwardCache().DisableForTesting(
        content::BackForwardCache::TEST_USES_UNLOAD_EVENT);

    const GURL a_url = https_test_server().GetURL("a.com", "/title1.html");
    const GURL b_url = https_test_server().GetURL("b.com", "/title2.html");

    // Navigate to a mostly empty page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));

    ChromePasswordManagerClient* client =
        ChromePasswordManagerClient::FromWebContents(WebContents());

    if (preestablish_mojo_pipe) {
      EXPECT_FALSE(client->has_binding_for_credential_manager());
      ASSERT_NO_FATAL_FAILURE(
          TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(
              WebContents(), false));
      EXPECT_TRUE(client->has_binding_for_credential_manager());
    }

    // Schedule storing a credential on the `unload` event.
    ASSERT_NO_FATAL_FAILURE(ScheduleNavigatorStoreCredentialAtUnload(
        WebContents(), "user", "hunter2"));

    // Trigger a cross-site navigation that is carried out in a new renderer,
    // and which will swap out the old RenderFrameHost.
    content::RenderFrameDeletedObserver rfh_destruction_observer(
        WebContents()->GetPrimaryMainFrame());
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));

    // Ensure that the navigator.credentials.store() call is never serviced.
    // The sufficient conditions for this are:
    //  -- The swapped out RFH is destroyed, so the RenderFrame cannot
    //     establish a new Mojo connection to ContentCredentialManager anymore.
    //  -- There is no pre-existing Mojo connection to ContentCredentialManager
    //     either, which could be used to call store() in the future.
    //  -- There have not been any calls to store() in the past.
    rfh_destruction_observer.WaitUntilDeleted();
    EXPECT_FALSE(client->has_binding_for_credential_manager());
    EXPECT_FALSE(client->was_store_ever_called());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests.

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AccountChooserWithOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click'.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'password';";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(
      content::ExecJs(WebContents(),
                      "navigator.credentials.get({password: true})"
                      ".then(cred => window.location = '/password/done.html')",
                      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())
      ->ChooseCredential(
          signin_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  PasswordsNavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(observer.Wait());

  // Verify that the form's 'skip_zero_click' is updated and not overwritten
  // by the autofill password manager on successful login.
  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap passwords_map =
      password_store->stored_passwords();
  ASSERT_EQ(1u, passwords_map.size());
  const std::vector<password_manager::PasswordForm>& passwords_vector =
      passwords_map.begin()->second;
  ASSERT_EQ(1u, passwords_vector.size());
  const password_manager::PasswordForm& form = passwords_vector[0];
  EXPECT_EQ(u"user", form.username_value);
  EXPECT_EQ(u"password", form.password_value);
  EXPECT_FALSE(form.skip_zero_click);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreExistingCredentialIsNoOp) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  GURL url = embedded_test_server()->base_url();

  password_manager::PasswordForm form_1;
  form_1.signon_realm = url.spec();
  form_1.url = url;
  form_1.username_value = u"user1";
  form_1.password_value = u"abcdef";

  password_manager::PasswordForm form_2;
  form_2.signon_realm = url.spec();
  form_2.url = url;
  form_2.username_value = u"user2";
  form_2.password_value = u"123456";

  password_store->AddLogin(form_1);
  password_store->AddLogin(form_2);
  WaitForPasswordStore();

  // Check that the password store contains the values we expect.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"abcdef", passwords[0].password_value);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"123456", passwords[1].password_value);
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user1' with the old password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user1', password: 'abcdef' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user2' with the old password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user2', password: '123456' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }
  // Wait for the password store to process the store request.
  WaitForPasswordStore();

  // Check that the password still store contains the values we expect.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"abcdef", passwords[0].password_value);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"123456", passwords[1].password_value);
  }
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreUpdatesPasswordOfExistingCredential) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  GURL url = embedded_test_server()->base_url();

  password_manager::PasswordForm form_1;
  form_1.signon_realm = url.spec();
  form_1.url = url;
  form_1.username_value = u"user1";
  form_1.password_value = u"abcdef";

  password_manager::PasswordForm form_2;
  form_2.signon_realm = url.spec();
  form_2.url = url;
  form_2.username_value = u"user2";
  form_2.password_value = u"123456";

  password_store->AddLogin(form_1);
  password_store->AddLogin(form_2);
  WaitForPasswordStore();

  // Check that the password store contains the values we expect.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"abcdef", passwords[0].password_value);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"123456", passwords[1].password_value);
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user1' with a new password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user1', password: 'ABCDEF' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user2' with a new password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user2', password: 'UVWXYZ' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }

  // Wait for the password store to process the store request.
  WaitForPasswordStore();

  // Check that the password store contains the values we expect.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"ABCDEF", passwords[0].password_value);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"UVWXYZ", passwords[1].password_value);
  }
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreUpdatesPasswordOfExistingCredentialWithAttributes) {
  // This test is the same as the previous one, except that the already existing
  // credentials contain metadata.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  GURL url = embedded_test_server()->base_url();

  password_manager::PasswordForm form_1;
  form_1.signon_realm = url.spec();
  form_1.username_value = u"user1";
  form_1.password_value = u"abcdef";
  form_1.username_element = u"user";
  form_1.password_element = u"pass";
  form_1.url = GURL(url.spec() + "/my/custom/path/");

  password_manager::PasswordForm form_2;
  form_2.signon_realm = url.spec();
  form_2.username_value = u"user2";
  form_2.password_value = u"123456";
  form_2.username_element = u"username";
  form_2.password_element = u"password";
  form_2.url = GURL(url.spec() + "/my/other/path/");

  password_store->AddLogin(form_1);
  password_store->AddLogin(form_2);
  WaitForPasswordStore();

  // Check that the password store contains the values we expect.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"abcdef", passwords[0].password_value);
    EXPECT_EQ(u"user", passwords[0].username_element);
    EXPECT_EQ(u"pass", passwords[0].password_element);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"123456", passwords[1].password_value);
    EXPECT_EQ(u"username", passwords[1].username_element);
    EXPECT_EQ(u"password", passwords[1].password_element);
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user1' with a new password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user1', password: 'ABCDEF' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }

  {
    NavigateToFile("/password/simple_password.html");

    // Call the API to store 'user2' with a new password.
    ASSERT_TRUE(content::ExecJs(
        WebContents(),
        "navigator.credentials.store("
        "  new PasswordCredential({ id: 'user2', password: 'UVWXYZ' }))"
        ".then(cred => window.location = '/password/done.html');",
        content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

    PasswordsNavigationObserver observer(WebContents());
    observer.SetPathToWaitFor("/password/done.html");
    ASSERT_TRUE(observer.Wait());
  }

  // Wait for the password store to process the store request.
  WaitForPasswordStore();

  // Check that the password store contains the values we expect.
  // Note that we don't check for username and password elements, as they don't
  // exist for credentials saved by the API.
  {
    auto found = password_store->stored_passwords().find(url.spec());
    ASSERT_NE(password_store->stored_passwords().end(), found);
    const std::vector<password_manager::PasswordForm>& passwords =
        found->second;

    ASSERT_EQ(2U, passwords.size());
    EXPECT_EQ(u"user1", passwords[0].username_value);
    EXPECT_EQ(u"ABCDEF", passwords[0].password_value);
    EXPECT_EQ(u"user2", passwords[1].username_value);
    EXPECT_EQ(u"UVWXYZ", passwords[1].password_value);
  }
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreSavesPSLMatchedCredential) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // The call to |GetURL| is needed to get the correct port.
  GURL psl_url = https_test_server().GetURL("psl.example.com", "/");

  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = psl_url.spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = psl_url;
  password_store->AddLogin(signin_form);

  NavigateToURL(https_test_server(), "www.example.com",
                "/password/password_form.html");

  // Call the API to trigger |get| and |store| and redirect.
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})"
                              ".then(cred => "
                              "navigator.credentials.store(cred)"
                              ".then(cred => "
                              "window.location = '/password/done.html'))",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Mojo calls from the renderer are asynchronous.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  PasswordsModelDelegateFromWebContents(WebContents())
      ->ChooseCredential(
          signin_form,
          password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);

  PasswordsNavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(observer.Wait());

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());

  // There should be an entry for both psl.example.com and www.example.com.
  password_manager::TestPasswordStore::PasswordMap passwords =
      password_store->stored_passwords();
  GURL www_url = https_test_server().GetURL("www.example.com", "/");
  EXPECT_EQ(2U, passwords.size());
  EXPECT_TRUE(base::Contains(passwords, psl_url.spec()));
  EXPECT_TRUE(base::Contains(passwords, www_url.spec()));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       UpdatingPSLMatchedCredentialCreatesSecondEntry) {
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());

  // The call to |GetURL| is needed to get the correct port.
  GURL psl_url = https_test_server().GetURL("psl.example.com", "/");

  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = psl_url.spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = psl_url;
  password_store->AddLogin(signin_form);

  NavigateToURL(https_test_server(), "www.example.com",
                "/password/password_form.html");

  // Call the API to trigger |get| and |store| and redirect.
  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "navigator.credentials.store("
      "  new PasswordCredential({ id: 'user', password: 'P4SSW0RD' }))"
      ".then(cred => window.location = '/password/done.html');",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  PasswordsNavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(observer.Wait());

  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();
  ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();
  WaitForPasswordStore();

  // There should be an entry for both psl.example.com and www.example.com.
  password_manager::TestPasswordStore::PasswordMap passwords =
      password_store->stored_passwords();
  GURL www_url = https_test_server().GetURL("www.example.com", "/");
  EXPECT_EQ(2U, passwords.size());
  EXPECT_TRUE(base::Contains(passwords, psl_url.spec()));
  EXPECT_TRUE(base::Contains(passwords, www_url.spec()));
  EXPECT_EQ(u"user", passwords[psl_url.spec()].front().username_value);
  EXPECT_EQ(u"password", passwords[psl_url.spec()].front().password_value);
  EXPECT_EQ(u"user", passwords[www_url.spec()].front().username_value);
  EXPECT_EQ(u"P4SSW0RD", passwords[www_url.spec()].front().password_value);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       ObsoleteHttpCredentialMovedOnMigrationToHstsSite) {
  // Add an http credential to the password store.
  GURL https_origin = https_test_server().base_url();
  ASSERT_TRUE(https_origin.SchemeIs(url::kHttpsScheme));
  GURL::Replacements rep;
  rep.SetSchemeStr(url::kHttpScheme);
  GURL http_origin = https_origin.ReplaceComponents(rep);
  password_manager::PasswordForm http_form;
  http_form.signon_realm = http_origin.spec();
  http_form.url = http_origin;
  http_form.username_value = u"user";
  http_form.password_value = u"12345";
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_store->AddLogin(http_form);
  WaitForPasswordStore();

  // Treat the host of the HTTPS test server as HSTS.
  AddHSTSHost(https_test_server().host_port_pair().host());

  // Navigate to HTTPS page and trigger the migration.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server().GetURL("/password/done.html")));

  // Call the API to trigger the account chooser.
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  BubbleObserver(WebContents()).WaitForAccountChooser();

  // Wait for the migration logic to actually touch the password store.
  WaitForPasswordStore();
  // Only HTTPS passwords should be present.
  EXPECT_THAT(password_store->stored_passwords(),
              ElementsAre(Pair(https_origin.spec(), SizeIs(1))));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       AutoSigninOldCredentialAndNavigation) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = false;
  password_store->AddLogin(signin_form);

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  NavigateToFile("/password/password_form.html");
  std::string fill_password =
      "document.getElementById('username_field').value = 'trash';"
      "document.getElementById('password_field').value = 'trash';";
  ASSERT_TRUE(content::ExecJs(WebContents(), fill_password));

  // Call the API to trigger the notification to the client.
  ASSERT_TRUE(
      content::ExecJs(WebContents(),
                      "navigator.credentials.get({password: true})"
                      ".then(cred => window.location = '/password/done.html');",
                      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  PasswordsNavigationObserver observer(WebContents());
  observer.SetPathToWaitFor("/password/done.html");
  ASSERT_TRUE(observer.Wait());

  BubbleObserver prompt_observer(WebContents());
  // The autofill password manager shouldn't react to the successful login
  // because it was suppressed when the site got the credential back.
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_SameSite_OnDemandMojoPipe) {
  TestStoreInUnloadHandlerForSameSiteNavigation(
      false /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_SameSite_PreestablishedPipe) {
  TestStoreInUnloadHandlerForSameSiteNavigation(
      true /* preestablish_mojo_pipe */);
}
// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_CrossSite_OnDemandMojoPipe) {
  TestStoreInUnloadHandlerForCrossSiteNavigation(
      false /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       StoreInUnloadHandler_CrossSite_PreestablishedPipe) {
  TestStoreInUnloadHandlerForCrossSiteNavigation(
      true /* preestablish_mojo_pipe */);
}

// Regression test for https://crbug.com/736357.
IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest,
                       MojoConnectionRecreatedAfterNavigation) {
  const GURL a_url1 = https_test_server().GetURL("a.com", "/title1.html");
  const GURL a_url2 = https_test_server().GetURL("a.com", "/title2.html");
  const GURL a_url2_ref = https_test_server().GetURL("a.com", "/title2.html#r");
  const GURL b_url = https_test_server().GetURL("b.com", "/title2.html#ref");

  // Enable 'auto signin' for the profile.
  password_bubble_experiment::RecordAutoSignInPromptFirstRunExperienceWasShown(
      browser()->profile()->GetPrefs());

  // Navigate to a mostly empty page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url1));

  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(WebContents());

  // Store a credential, and expect it to establish the Mojo connection.
  EXPECT_FALSE(client->has_binding_for_credential_manager());
  EXPECT_FALSE(client->was_store_ever_called());

  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "var c = new PasswordCredential({ id: 'user', password: 'hunter2' });"
      "navigator.credentials.store(c);",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();
  ASSERT_TRUE(prompt_observer.IsSavePromptShownAutomatically());
  prompt_observer.AcceptSavePrompt();
  WaitForPasswordStore();

  EXPECT_TRUE(client->has_binding_for_credential_manager());
  EXPECT_TRUE(client->was_store_ever_called());

  // Trigger a same-site navigation.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url2));

  // Expect the Mojo connection closed.
  EXPECT_FALSE(client->has_binding_for_credential_manager());

  // Calling navigator.credentials.get() again should re-establish the Mojo
  // connection and succeed.
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));
  EXPECT_TRUE(client->has_binding_for_credential_manager());

  // Same-document navigation. Call to get() succeeds.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url2_ref));
  EXPECT_TRUE(client->has_binding_for_credential_manager());
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));

  // Cross-site navigation. Call to get() succeeds without results.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               false));

  // Trigger a cross-site navigation back. Call to get() should still succeed,
  // and once again with results.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url1));
  ASSERT_NO_FATAL_FAILURE(
      TriggerNavigatorGetPasswordCredentialsAndExpectHasResult(WebContents(),
                                                               true));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, SaveViaAPIAndAutofill) {
  NavigateToFile("/password/password_form.html");
  const GURL current_url = WebContents()->GetLastCommittedURL();

  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
      "var c = new PasswordCredential({ id: 'user', password: 'API' });"
      "navigator.credentials.store(c);"
      "});"));
  // Fill the password and click the button to submit the page. The API should
  // suppress the autofill password manager.
  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));

  BubbleObserver prompt_observer(WebContents());
  prompt_observer.WaitForAutomaticSavePrompt();
  prompt_observer.AcceptSavePrompt();

  WaitForPasswordStore();
  password_manager::TestPasswordStore::PasswordMap stored =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get())
          ->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  password_manager::PasswordForm signin_form = stored.begin()->second[0];
  EXPECT_EQ(u"user", signin_form.username_value);
  EXPECT_EQ(u"API", signin_form.password_value);
  EXPECT_EQ(embedded_test_server()->base_url().spec(),
            signin_form.signon_realm);
  EXPECT_EQ(current_url.DeprecatedGetOriginAsURL(), signin_form.url);
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, UpdateViaAPIAndAutofill) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"old_pass";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  // Set an old value for the |date_last_used| to make sure it gets updated.
  signin_form.date_last_used = base::Time::UnixEpoch();
  password_store->AddLogin(signin_form);

  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "document.getElementById('input_submit_button').addEventListener('click',"
      "function(event) {"
      "var c = new PasswordCredential({ id: 'user', password: 'API' });"
      "navigator.credentials.store(c);"
      "});"));
  // Fill the new password and click the button to submit the page later. The
  // API should suppress the autofill password manager and overwrite the
  // password.
  PasswordsNavigationObserver form_submit_observer(WebContents());
  ASSERT_TRUE(content::ExecJs(
      WebContents(),
      "document.getElementById('username_field').value = 'user';"
      "document.getElementById('password_field').value = 'autofill';"
      "document.getElementById('input_submit_button').click();"));
  ASSERT_TRUE(form_submit_observer.Wait());

  // Wait for the password store before checking the prompt because it pops up
  // after the store replies.
  WaitForPasswordStore();
  BubbleObserver prompt_observer(WebContents());
  EXPECT_FALSE(prompt_observer.IsSavePromptShownAutomatically());
  EXPECT_FALSE(prompt_observer.IsUpdatePromptShownAutomatically());
  signin_form.match_type = password_manager::PasswordForm::MatchType::kExact;
  signin_form.skip_zero_click = false;
  signin_form.times_used_in_html_form = 1;
  signin_form.password_value = u"API";
  password_manager::TestPasswordStore::PasswordMap stored =
      password_store->stored_passwords();
  ASSERT_EQ(1u, stored.size());
  // Upon an update, the |date_last_used| should be updated to the current
  // timestamp.
  EXPECT_GT(stored[signin_form.signon_realm][0].date_last_used,
            signin_form.date_last_used);
  EXPECT_NE(stored[signin_form.signon_realm][0].date_password_modified,
            base::Time());
  // Now make them equal to be able to check the equality of other fields.
  signin_form.date_last_used =
      stored[signin_form.signon_realm][0].date_last_used;
  signin_form.date_password_modified =
      stored[signin_form.signon_realm][0].date_password_modified;
  EXPECT_THAT(signin_form,
              MatchesFormExceptStore(stored[signin_form.signon_realm][0]));
}

IN_PROC_BROWSER_TEST_F(CredentialManagerBrowserTest, CredentialsAutofilled) {
  NavigateToFile("/password/password_form.html");

  ASSERT_TRUE(content::ExecJs(
      RenderFrameHost(),
      "var c = new PasswordCredential({ id: 'user', password: '12345' });"
      "navigator.credentials.store(c);",
      content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  BubbleObserver bubble_observer(WebContents());
  bubble_observer.WaitForAutomaticSavePrompt();
  bubble_observer.AcceptSavePrompt();

  // Reload the page and make sure it's autofilled.
  NavigateToFile("/password/password_form.html");
  content::SimulateMouseClickAt(
      WebContents(), 0, blink::WebMouseEvent::Button::kLeft, gfx::Point(1, 1));
  WaitForElementValue("username_field", "user");
  WaitForElementValue("password_field", "12345");
}

class CredentialManagerPrerenderBrowserTest
    : public CredentialManagerBrowserTest {
 public:
  CredentialManagerPrerenderBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &CredentialManagerPrerenderBrowserTest::WebContents,
            base::Unretained(this))) {}
  ~CredentialManagerPrerenderBrowserTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    CredentialManagerBrowserTest::SetUp();
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return &prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Tests that binding mojom::CredentialManager doesn't work in the prerendering.
IN_PROC_BROWSER_TEST_F(CredentialManagerPrerenderBrowserTest,
                       BindCredentialManagerInPrerender) {
  // Save credentials with 'skip_zero_click' false.
  scoped_refptr<password_manager::TestPasswordStore> password_store =
      static_cast<password_manager::TestPasswordStore*>(
          ProfilePasswordStoreFactory::GetForProfile(
              browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS)
              .get());
  password_manager::PasswordForm signin_form;
  signin_form.signon_realm = embedded_test_server()->base_url().spec();
  signin_form.password_value = u"password123";
  signin_form.username_value = u"user";
  signin_form.url = embedded_test_server()->base_url();
  signin_form.skip_zero_click = true;
  password_store->AddLogin(signin_form);

  GURL url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  auto prerender_url =
      embedded_test_server()->GetURL("/password/credentials.html");
  // Loads a page in the prerender.
  content::FrameTreeNodeId host_id =
      prerender_helper()->AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*WebContents(), host_id);

  // It should not have binding mojom::CredentialManager.
  EXPECT_FALSE(ChromePasswordManagerClient::FromWebContents(WebContents())
                   ->has_binding_for_credential_manager());

  // Navigates the primary page to the URL.
  prerender_helper()->NavigatePrimaryPage(prerender_url);

  // The API should work.
  BubbleObserver(WebContents()).WaitForAccountChooser();

  // Make sure that the prerender was activated.
  EXPECT_TRUE(host_observer.was_activated());
  // After the page is activated, it gets binding mojom::CredentialManager.
  EXPECT_TRUE(ChromePasswordManagerClient::FromWebContents(WebContents())
                  ->has_binding_for_credential_manager());
}

class CredentialManagerAvatarTest : public PasswordManagerBrowserTestBase {
 public:
  static const char kAvatarOrigin[];
  static const char kAvatarPath[];
  static const char kLoginPath[];

  CredentialManagerAvatarTest() {
    https_test_server().RegisterRequestHandler(base::BindRepeating(
        &CredentialManagerAvatarTest::HandleRequest, base::Unretained(this)));
  }

  // Add a Credential Management API password with an icon to the store.
  void AddPasswordForURL(const GURL& url);

  // A counter for requests made to fetch the avatar.
  size_t avatar_request_counter() const { return avatar_request_counter_; }

  // Runs the message loop until the avatar counter equals |expected|.
  void WaitForAvatarCounter(size_t expected);

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request);
  void OnIncrementAvatarCounter();

  size_t avatar_request_counter_ = 0;

  // A pointer to the run loop used to wait for the avatar.
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};

const char CredentialManagerAvatarTest::kAvatarOrigin[] = "avatarserver.com";
const char CredentialManagerAvatarTest::kAvatarPath[] = "/image.png";
const char CredentialManagerAvatarTest::kLoginPath[] = "/login";

void CredentialManagerAvatarTest::AddPasswordForURL(const GURL& url) {
  password_manager::PasswordForm form;
  form.url = url;
  form.signon_realm = form.url.GetWithEmptyPath().spec();
  form.username_value = u"User";
  form.password_value = u"12345";
  form.type = password_manager::PasswordForm::Type::kApi;
  form.skip_zero_click = true;
  form.icon_url = https_test_server().GetURL(kAvatarOrigin, kAvatarPath);

  scoped_refptr<password_manager::PasswordStoreInterface> password_store =
      ProfilePasswordStoreFactory::GetForProfile(
          browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);
  password_store->AddLogin(form);
}

void CredentialManagerAvatarTest::WaitForAvatarCounter(size_t expected) {
  if (avatar_request_counter_ == expected) {
    return;
  }
  // The logic doesn't support increments by more than one.
  EXPECT_EQ(expected, avatar_request_counter_ + 1);
  base::RunLoop loop;
  run_loop_ = &loop;
  loop.Run();
  EXPECT_EQ(expected, avatar_request_counter_);
}

std::unique_ptr<net::test_server::HttpResponse>
CredentialManagerAvatarTest::HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (request.relative_url == kAvatarPath) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("image/gif");
    http_response->AddCustomHeader("Cache-Control", "max-age=100000");
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CredentialManagerAvatarTest::OnIncrementAvatarCounter,
                       base::Unretained(this)));
  } else if (request.relative_url == kLoginPath) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/plain");
    http_response->set_content("Login now");
  }
  return http_response;
}

void CredentialManagerAvatarTest::OnIncrementAvatarCounter() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  avatar_request_counter_++;
  if (run_loop_) {
    run_loop_->Quit();
    run_loop_ = nullptr;
  }
}

// Test that the avatar is requested in the context of the main frame. Thus,
// it should not be cached by one origin for another origin.
IN_PROC_BROWSER_TEST_F(CredentialManagerAvatarTest,
                       AvatarFetchIsolatedPerOrigin) {
  const GURL a_url = https_test_server().GetURL("a.com", kLoginPath);
  const GURL b_url = https_test_server().GetURL("b.com", kLoginPath);

  AddPasswordForURL(a_url);
  AddPasswordForURL(b_url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // The account chooser UI requested the avatar.
  BubbleObserver(WebContents()).WaitForAccountChooser();
  WaitForAvatarCounter(1u);

  // Navigate to the second site, the icon is requested again.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), b_url));
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  BubbleObserver(WebContents()).WaitForAccountChooser();
  WaitForAvatarCounter(2u);

  // Navigate back to the first site, the icon is already cached.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), a_url));
  ASSERT_TRUE(content::ExecJs(WebContents(),
                              "navigator.credentials.get({password: true})",
                              content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  BubbleObserver(WebContents()).WaitForAccountChooser();
  EXPECT_EQ(avatar_request_counter(), 2u);
}

}  // namespace
