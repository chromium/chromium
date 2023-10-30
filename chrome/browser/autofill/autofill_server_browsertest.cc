// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64url.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/third_party/mozilla/url_parse.h"

using testing::AllOf;
using testing::Eq;
using testing::Matcher;
using testing::Property;
using version_info::GetProductNameAndVersionForUserAgent;

namespace autofill {
namespace {

// TODO(bondd): PdmChangeWaiter in autofill_uitest_util.cc is a replacement for
// this class. Remove this class and use helper functions in that file instead.
class WindowedPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Profile* profile)
      : profile_(profile),
        message_loop_runner_(new content::MessageLoopRunner) {
    PersonalDataManagerFactory::GetForProfile(profile_)->AddObserver(this);
  }
  ~WindowedPersonalDataManagerObserver() override {}

  // Waits for the PersonalDataManager's list of profiles to be updated.
  void Wait() {
    message_loop_runner_->Run();
    PersonalDataManagerFactory::GetForProfile(profile_)->RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override { message_loop_runner_->Quit(); }

 private:
  raw_ptr<Profile> profile_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

class WindowedNetworkObserver {
 public:
  explicit WindowedNetworkObserver(Matcher<std::string> expected_upload_data)
      : expected_upload_data_(expected_upload_data),
        message_loop_runner_(new content::MessageLoopRunner) {
    interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            &WindowedNetworkObserver::OnIntercept, base::Unretained(this)));
  }

  WindowedNetworkObserver(const WindowedNetworkObserver&) = delete;
  WindowedNetworkObserver& operator=(const WindowedNetworkObserver&) = delete;

  ~WindowedNetworkObserver() {}

  // Waits for a network request with the |expected_upload_data_|.
  void Wait() {
    message_loop_runner_->Run();
    interceptor_.reset();
  }

 private:
  // Helper to extract the value passed to a lookup in the URL. Returns "*** not
  // found ***" if the the data cannot be decoded.
  std::string GetLookupContent(const std::string& query_path) {
    if (query_path.find("/v1/pages/") == std::string::npos)
      return "*** not found ***";
    std::string payload = query_path.substr(strlen("/v1/pages/"));
    std::string decoded_payload;
    if (base::Base64UrlDecode(payload,
                              base::Base64UrlDecodePolicy::REQUIRE_PADDING,
                              &decoded_payload)) {
      return decoded_payload;
    }
    return "*** not found ***";
  }

  bool OnIntercept(content::URLLoaderInterceptor::RequestParams* params) {
    // NOTE: This constant matches the one defined in
    // components/autofill/core/browser/autofill_download_manager.cc
    static const char kDefaultAutofillServerURL[] =
        "https://content-autofill.googleapis.com/";
    DCHECK(params);
    network::ResourceRequest resource_request = params->url_request;
    if (resource_request.url.spec().find(kDefaultAutofillServerURL) ==
        std::string::npos) {
      return false;
    }

    const std::string& data =
        (resource_request.method == "GET")
            ? GetLookupContent(resource_request.url.path())
            : network::GetUploadData(resource_request);

    if (expected_upload_data_.Matches(data))
      message_loop_runner_->Quit();

    return false;
  }

 private:
  Matcher<std::string> expected_upload_data_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

}  // namespace

class AutofillServerTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    // Enable data-url support.
    // TODO(crbug.com/894428) - fix this suite to use the embedded test server
    // instead of data urls.
    scoped_feature_list_.InitWithFeatures(
        // Enabled.
        {features::test::kAutofillAllowNonHttpActivation,
         features::test::kAutofillServerCommunication},
        // Disabled.
        {});

    // Note that features MUST be enabled/disabled before continuing with
    // SetUp(); otherwise, the feature state doesn't propagate to the test
    // browser instance.
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    // Wait for Personal Data Manager to be fully loaded as the events about
    // being loaded may throw off the tests and cause flakiness.
    WaitForPersonalDataManagerToBeLoaded(browser()->profile());
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

MATCHER_P(EqualsUploadProto, expected_const, "") {
  AutofillUploadRequest expected = expected_const;
  AutofillUploadRequest request;
  if (!request.ParseFromString(arg))
    return false;

  // Remove metadata because it is randomized and won't match.
  EXPECT_EQ(request.upload().has_randomized_form_metadata(),
            expected.upload().has_randomized_form_metadata());
  request.mutable_upload()->clear_randomized_form_metadata();
  expected.mutable_upload()->clear_randomized_form_metadata();
  EXPECT_EQ(request.upload().field_size(), expected.upload().field_size());
  if (request.upload().field_size() != expected.upload().field_size())
    return false;
  for (int i = 0; i < request.upload().field_size(); i++) {
    request.mutable_upload()
        ->mutable_field(i)
        ->clear_randomized_field_metadata();
    expected.mutable_upload()
        ->mutable_field(i)
        ->clear_randomized_field_metadata();
  }

  // TODO(crbug.com/1251119): The language is sometimes missing from the upload,
  // making the test flaky. Add the language back to the comparison when the
  // root cause is fixed.
  request.mutable_upload()->clear_language();
  expected.mutable_upload()->clear_language();

  return request.SerializeAsString() == expected.SerializeAsString();
}

// Regression test for http://crbug.com/177419
IN_PROC_BROWSER_TEST_F(AutofillServerTest,
                       QueryAndUploadBothIncludeFieldsWithAutocompleteOff) {
  // Seed some test Autofill profile data, as upload requests are only made when
  // there is local data available to use as a baseline.
  WindowedPersonalDataManagerObserver personal_data_observer(
      browser()->profile());
  PersonalDataManagerFactory::GetForProfile(browser()->profile())
      ->AddProfile(test::GetFullProfile());
  personal_data_observer.Wait();

  // Load the test page. Expect a query request upon loading the page.
  const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
  const char kFormHtml[] =
      "<form id='test_form' action='about:blank'>"
      "  <input name='one'>"
      "  <input name='two' autocomplete='off'>"
      "  <input name='three'>"
      "  <input name='four' autocomplete='off'>"
      "  <input type='submit'>"
      "</form>"
      "<script>"
      "  document.onclick = function() {"
      "    document.getElementById('test_form').submit();"
      "  };"
      "</script>";

  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  auto* query_form = query.add_forms();
  query_form->set_signature(15916856893790176210U);
  query_form->set_alternative_signature(1512434549531087U);

  query_form->add_fields()->set_signature(2594484045U);
  query_form->add_fields()->set_signature(2750915947U);
  query_form->add_fields()->set_signature(3494787134U);
  query_form->add_fields()->set_signature(1236501728U);

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  WindowedNetworkObserver query_network_observer(expected_query_string);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kFormHtml)));
  query_network_observer.Wait();

  // Submit the form, using a simulated mouse click because form submissions not
  // triggered by user gestures are ignored. Expect an upload request upon form
  // submission, with form fields matching those from the query request.
  AutofillUploadRequest request;
  AutofillUploadContents* upload = request.mutable_upload();
  upload->set_submission(true);
  upload->set_client_version(
      std::string(GetProductNameAndVersionForUserAgent()));
  upload->set_form_signature(15916856893790176210U);
  upload->set_autofill_used(false);

  // The `data_present` fields is a bit mask of field types that are associated
  // with non-empty profile values. Each bit in this mask corresponds to a
  // specific type. For details on that mapping please consult
  // |EncodeFieldTypes()| in components/autofill/core/browser/form_structure.cc.
  // The resulting bit mask in this test is hard-coded to capture regressions in
  // the calculation of the mask.

  std::string data_present;
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForHonorificPrefixes)) {
    data_present = "1f7e0003f80000080004000001c424180002";
  } else {
    data_present = "1f7e0003f80000080004000001c420180002";
  }

  // TODO(crbug.com/1311937): Additional phone number trunk types are present
  // if AutofillEnableSupportForPhoneNumberTrunkTypes is enabled. Clean-up
  // implementation when launched.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableSupportForPhoneNumberTrunkTypes)) {
    data_present.rbegin()[5] = '7';
  }
  upload->set_data_present(data_present);

  upload->set_passwords_revealed(false);
  upload->set_submission_event(
      AutofillUploadContents_SubmissionIndicatorEvent_HTML_FORM_SUBMISSION);
  upload->set_has_form_tag(true);
  // We don't set metadata, because the matcher will skip them.
  upload->set_language("und");
  *upload->mutable_randomized_form_metadata() =
      autofill::AutofillRandomizedFormMetadata();

  // Enabling raw form data uploading (e.g., field name) is too complicated in
  // this test. So, don't expect it in the upload.
  test::FillUploadField(upload->add_field(), 2594484045U, 2U);
  test::FillUploadField(upload->add_field(), 2750915947U, 2U);
  test::FillUploadField(upload->add_field(), 3494787134U, 2U);
  test::FillUploadField(upload->add_field(), 1236501728U, 2U);

  WindowedNetworkObserver upload_network_observer(EqualsUploadProto(request));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::SimulateMouseClick(web_contents, 0,
                              blink::WebMouseEvent::Button::kLeft);
  upload_network_observer.Wait();
}

// Verify that a site with password fields will query even in the presence
// of user defined autocomplete types.
IN_PROC_BROWSER_TEST_F(AutofillServerTest, AlwaysQueryForPasswordFields) {
  // Load the test page. Expect a query request upon loading the page.
  const char kDataURIPrefix[] = "data:text/html;charset=utf-8,";
  const char kFormHtml[] =
      "<form id='test_form'>"
      "  <input type='text' id='one' autocomplete='username'>"
      "  <input type='text' id='two' autocomplete='off'>"
      "  <input type='password' id='three'>"
      "  <input type='submit'>"
      "</form>";

  AutofillPageQueryRequest query;
  query.set_client_version(std::string(GetProductNameAndVersionForUserAgent()));
  auto* query_form = query.add_forms();
  query_form->set_signature(8900697631820480876U);
  query_form->set_alternative_signature(8962829409320837774U);

  query_form->add_fields()->set_signature(2594484045U);
  query_form->add_fields()->set_signature(2750915947U);
  query_form->add_fields()->set_signature(116843943U);

  std::string expected_query_string;
  ASSERT_TRUE(query.SerializeToString(&expected_query_string));

  WindowedNetworkObserver query_network_observer(expected_query_string);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(std::string(kDataURIPrefix) + kFormHtml)));
  query_network_observer.Wait();
}

}  // namespace autofill
