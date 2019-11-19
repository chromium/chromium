// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"

using base::ASCIIToUTF16;
using base::UTF16ToASCII;

namespace autofill {

// Default JavaScript code used to submit the forms.
const char kDocumentClickHandlerSubmitJS[] =
    "document.onclick = function() {"
    "  document.getElementById('testform').submit();"
    "};";

// TODO(bondd): PdmChangeWaiter in autofill_uitest_util.cc is a replacement for
// this class. Remove this class and use helper functions in that file instead.
class WindowedPersonalDataManagerObserver : public PersonalDataManagerObserver {
 public:
  explicit WindowedPersonalDataManagerObserver(Browser* browser)
      : alerted_(false), has_run_message_loop_(false), browser_(browser) {
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        AddObserver(this);
  }

  ~WindowedPersonalDataManagerObserver() override {}

  void Wait() {
    if (!alerted_) {
      has_run_message_loop_ = true;
      content::RunMessageLoop();
    }
    PersonalDataManagerFactory::GetForProfile(browser_->profile())->
        RemoveObserver(this);
  }

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override {
    if (has_run_message_loop_) {
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
      has_run_message_loop_ = false;
    }
    alerted_ = true;
  }

  void OnInsufficientFormData() override { OnPersonalDataChanged(); }

 private:
  bool alerted_;
  bool has_run_message_loop_;
  Browser* browser_;
};

class AutofillTest : public InProcessBrowserTest {
 protected:
  AutofillTest() {}

  void SetUpOnMainThread() override {
    // Don't want Keychain coming up on Mac.
    test::DisableSystemServices(browser()->profile()->GetPrefs());

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    // Make sure to close any showing popups prior to tearing down the UI.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    AutofillManager* autofill_manager =
        ContentAutofillDriverFactory::FromWebContents(web_contents)
            ->DriverForFrame(web_contents->GetMainFrame())
            ->autofill_manager();
    autofill_manager->client()->HideAutofillPopup();
    test::ReenableSystemServices();
  }

  PersonalDataManager* personal_data_manager() {
    return PersonalDataManagerFactory::GetForProfile(browser()->profile());
  }

  typedef std::map<std::string, std::string> FormMap;

  // Helper function to obtain the Javascript required to update a form.
  std::string GetJSToFillForm(const FormMap& data) {
    std::string js;
    for (const auto& entry : data) {
      js += "document.getElementById('" + entry.first + "').value = '" +
            entry.second + "';";
    }
    return js;
  }

  // Navigate to the form, input values into the fields, and submit the form.
  // The function returns after the PersonalDataManager is updated.
  void FillFormAndSubmit(const std::string& filename, const FormMap& data) {
    FillFormAndSubmitWithHandler(filename, data, kDocumentClickHandlerSubmitJS,
                                 true);
  }

  // Helper where the actual submit JS code can be specified, as well as whether
  // the test should |simulate_click| on the document.
  void FillFormAndSubmitWithHandler(const std::string& filename,
                                    const FormMap& data,
                                    const std::string& submit_js,
                                    bool simulate_click) {
    GURL url = embedded_test_server()->GetURL("/autofill/" + filename);
    NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    ui_test_utils::NavigateToURL(&params);

    WindowedPersonalDataManagerObserver observer(browser());

    std::string js = GetJSToFillForm(data) + submit_js;
    ASSERT_TRUE(content::ExecuteScript(web_contents(), js));
    if (simulate_click) {
      // Simulate a mouse click to submit the form because form submissions not
      // triggered by user gestures are ignored.
      content::SimulateMouseClick(
          browser()->tab_strip_model()->GetActiveWebContents(), 0,
          blink::WebMouseEvent::Button::kLeft);
    }
    observer.Wait();
  }

  // Aggregate profiles from forms into Autofill preferences. Returns the number
  // of parsed profiles.
  int AggregateProfilesIntoAutofillPrefs(const std::string& filename) {
    std::string data;
    base::FilePath data_file =
        ui_test_utils::GetTestFilePath(base::FilePath().AppendASCII("autofill"),
                                       base::FilePath().AppendASCII(filename));
    CHECK(base::ReadFileToString(data_file, &data));
    std::vector<std::string> lines = base::SplitString(
        data, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    int parsed_profiles = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
      if (base::StartsWith(lines[i], "#", base::CompareCase::SENSITIVE))
        continue;

      std::vector<std::string> fields = base::SplitString(
          lines[i], "|", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      if (fields.empty())
        continue;  // Blank line.

      ++parsed_profiles;
      CHECK_EQ(12u, fields.size());

      FormMap data;
      data["NAME_FIRST"] = fields[0];
      data["NAME_MIDDLE"] = fields[1];
      data["NAME_LAST"] = fields[2];
      data["EMAIL_ADDRESS"] = fields[3];
      data["COMPANY_NAME"] = fields[4];
      data["ADDRESS_HOME_LINE1"] = fields[5];
      data["ADDRESS_HOME_LINE2"] = fields[6];
      data["ADDRESS_HOME_CITY"] = fields[7];
      data["ADDRESS_HOME_STATE"] = fields[8];
      data["ADDRESS_HOME_ZIP"] = fields[9];
      data["ADDRESS_HOME_COUNTRY"] = fields[10];
      data["PHONE_HOME_WHOLE_NUMBER"] = fields[11];

      FillFormAndSubmit("duplicate_profiles_test.html", data);
    }
    return parsed_profiles;
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
};

// Test that Autofill aggregates a minimum valid profile.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, AggregatesMinValidProfile) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());
}

// Different Javascript to submit the form.
IN_PROC_BROWSER_TEST_F(AutofillTest, AggregatesMinValidProfileDifferentJS) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";

  std::string submit("document.forms[0].submit();");
  FillFormAndSubmitWithHandler("duplicate_profiles_test.html", data, submit,
                               false);

  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());
}

// Form submitted via JavaScript, the user's personal data is updated even
// if the event handler on the submit event prevents submission of the form.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesAggregatedWithSubmitHandler) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";

  std::string submit(
      "var preventFunction = function(event) { event.preventDefault(); };"
      "document.forms[0].addEventListener('submit', preventFunction);"
      "document.querySelector('input[type=submit]').click();");
  FillFormAndSubmitWithHandler("duplicate_profiles_test.html", data, submit,
                               false);

  // The AutofillManager will update the user's profile.
  EXPECT_EQ(1u, personal_data_manager()->GetProfiles().size());

  EXPECT_EQ(ASCIIToUTF16("Bob"),
            personal_data_manager()->GetProfiles()[0]->GetRawInfo(NAME_FIRST));
  EXPECT_EQ(ASCIIToUTF16("Smith"),
            personal_data_manager()->GetProfiles()[0]->GetRawInfo(NAME_LAST));
}

// Test Autofill does not aggregate profiles with no address info.
// The minimum required address fields must be specified: First Name, Last Name,
// Address Line 1, City, Zip Code, and State.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithNoAddress) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "bsmith@example.com";
  data["COMPANY_NAME"] = "Mountain View";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["PHONE_HOME_WHOLE_NUMBER"] = "650-555-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->GetProfiles().empty());
}

// Test Autofill does not aggregate profiles with an invalid email.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfilesNotAggregatedWithInvalidEmail) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["EMAIL_ADDRESS"] = "garbage";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_TRUE(personal_data_manager()->GetProfiles().empty());
}

// Test profile is saved if phone number is valid in selected country.
// The data file contains two profiles with valid phone numbers and two
// profiles with invalid phone numbers from their respective country.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileSavedWithValidCountryPhone) {
  std::vector<FormMap> profiles;

  FormMap data1;
  data1["NAME_FIRST"] = "Bob";
  data1["NAME_LAST"] = "Smith";
  data1["ADDRESS_HOME_LINE1"] = "123 Cherry Ave";
  data1["ADDRESS_HOME_CITY"] = "Mountain View";
  data1["ADDRESS_HOME_STATE"] = "CA";
  data1["ADDRESS_HOME_ZIP"] = "94043";
  data1["ADDRESS_HOME_COUNTRY"] = "United States";
  data1["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  profiles.push_back(data1);

  FormMap data2;
  data2["NAME_FIRST"] = "John";
  data2["NAME_LAST"] = "Doe";
  data2["ADDRESS_HOME_LINE1"] = "987 H St";
  data2["ADDRESS_HOME_CITY"] = "San Jose";
  data2["ADDRESS_HOME_STATE"] = "CA";
  data2["ADDRESS_HOME_ZIP"] = "95510";
  data2["ADDRESS_HOME_COUNTRY"] = "United States";
  data2["PHONE_HOME_WHOLE_NUMBER"] = "408-123-456";
  profiles.push_back(data2);

  FormMap data3;
  data3["NAME_FIRST"] = "Jane";
  data3["NAME_LAST"] = "Doe";
  data3["ADDRESS_HOME_LINE1"] = "1523 Garcia St";
  data3["ADDRESS_HOME_CITY"] = "Mountain View";
  data3["ADDRESS_HOME_STATE"] = "CA";
  data3["ADDRESS_HOME_ZIP"] = "94043";
  data3["ADDRESS_HOME_COUNTRY"] = "Germany";
  data3["PHONE_HOME_WHOLE_NUMBER"] = "+49 40-80-81-79-000";
  profiles.push_back(data3);

  FormMap data4;
  data4["NAME_FIRST"] = "Bonnie";
  data4["NAME_LAST"] = "Smith";
  data4["ADDRESS_HOME_LINE1"] = "6723 Roadway Rd";
  data4["ADDRESS_HOME_CITY"] = "San Jose";
  data4["ADDRESS_HOME_STATE"] = "CA";
  data4["ADDRESS_HOME_ZIP"] = "95510";
  data4["ADDRESS_HOME_COUNTRY"] = "Germany";
  data4["PHONE_HOME_WHOLE_NUMBER"] = "+21 08450 777 777";
  profiles.push_back(data4);

  for (size_t i = 0; i < profiles.size(); ++i)
    FillFormAndSubmit("autofill_test_form.html", profiles[i]);

  ASSERT_EQ(2u, personal_data_manager()->GetProfiles().size());
  int us_address_index =
      personal_data_manager()->GetProfiles()[0]->GetRawInfo(
          ADDRESS_HOME_LINE1) == ASCIIToUTF16("123 Cherry Ave")
          ? 0
          : 1;

  EXPECT_EQ(
      ASCIIToUTF16("408-871-4567"),
      personal_data_manager()->GetProfiles()[us_address_index]->GetRawInfo(
          PHONE_HOME_WHOLE_NUMBER));
  ASSERT_EQ(
      ASCIIToUTF16("+49 40-80-81-79-000"),
      personal_data_manager()->GetProfiles()[1 - us_address_index]->GetRawInfo(
          PHONE_HOME_WHOLE_NUMBER));
}

// Prepend country codes when formatting phone numbers, but only if the user
// provided one in the first place.
IN_PROC_BROWSER_TEST_F(AutofillTest, AppendCountryCodeForAggregatedPhones) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["ADDRESS_HOME_COUNTRY"] = "Germany";
  data["PHONE_HOME_WHOLE_NUMBER"] = "+4908450777777";
  FillFormAndSubmit("autofill_test_form.html", data);

  data["ADDRESS_HOME_LINE1"] = "4321 H St.";
  data["PHONE_HOME_WHOLE_NUMBER"] = "08450777777";
  FillFormAndSubmit("autofill_test_form.html", data);

  ASSERT_EQ(2u, personal_data_manager()->GetProfiles().size());
  int second_address_index =
      personal_data_manager()->GetProfiles()[0]->GetRawInfo(
          ADDRESS_HOME_LINE1) == ASCIIToUTF16("4321 H St.")
          ? 0
          : 1;

  EXPECT_EQ(ASCIIToUTF16("+49 8450 777777"),
            personal_data_manager()
                ->GetProfiles()[1 - second_address_index]
                ->GetRawInfo(PHONE_HOME_WHOLE_NUMBER));

  EXPECT_EQ(
      ASCIIToUTF16("08450 777777"),
      personal_data_manager()->GetProfiles()[second_address_index]->GetRawInfo(
          PHONE_HOME_WHOLE_NUMBER));
}

// Test that Autofill uses '+' sign for international numbers.
// This applies to the following cases:
//   The phone number has a leading '+'.
//   The phone number does not have a leading '+'.
//   The phone number has a leading international direct dialing (IDD) code.
// This does not apply to US numbers. For US numbers, '+' is removed.

// Flaky on Windows. http://crbug.com/500491
// Also flaky on Linux. http://crbug.com/935629
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_UsePlusSignForInternationalNumber \
    DISABLED_UsePlusSignForInternationalNumber
#else
#define MAYBE_UsePlusSignForInternationalNumber \
    UsePlusSignForInternationalNumber
#endif

IN_PROC_BROWSER_TEST_F(AutofillTest, MAYBE_UsePlusSignForInternationalNumber) {
  std::vector<FormMap> profiles;

  FormMap data1;
  data1["NAME_FIRST"] = "Bonnie";
  data1["NAME_LAST"] = "Smith";
  data1["ADDRESS_HOME_LINE1"] = "6723 Roadway Rd";
  data1["ADDRESS_HOME_CITY"] = "Reading";
  data1["ADDRESS_HOME_STATE"] = "Berkshire";
  data1["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data1["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data1["PHONE_HOME_WHOLE_NUMBER"] = "+44 7624-123456";
  profiles.push_back(data1);

  FormMap data2;
  data2["NAME_FIRST"] = "John";
  data2["NAME_LAST"] = "Doe";
  data2["ADDRESS_HOME_LINE1"] = "987 H St";
  data2["ADDRESS_HOME_CITY"] = "Reading";
  data2["ADDRESS_HOME_STATE"] = "BerkShire";
  data2["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data2["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data2["PHONE_HOME_WHOLE_NUMBER"] = "44 7624 123456";
  profiles.push_back(data2);

  FormMap data3;
  data3["NAME_FIRST"] = "Jane";
  data3["NAME_LAST"] = "Doe";
  data3["ADDRESS_HOME_LINE1"] = "1523 Garcia St";
  data3["ADDRESS_HOME_CITY"] = "Reading";
  data3["ADDRESS_HOME_STATE"] = "BerkShire";
  data3["ADDRESS_HOME_ZIP"] = "RG12 3BR";
  data3["ADDRESS_HOME_COUNTRY"] = "United Kingdom";
  data3["PHONE_HOME_WHOLE_NUMBER"] = "0044 7624 123456";
  profiles.push_back(data3);

  FormMap data4;
  data4["NAME_FIRST"] = "Bob";
  data4["NAME_LAST"] = "Smith";
  data4["ADDRESS_HOME_LINE1"] = "123 Cherry Ave";
  data4["ADDRESS_HOME_CITY"] = "Mountain View";
  data4["ADDRESS_HOME_STATE"] = "CA";
  data4["ADDRESS_HOME_ZIP"] = "94043";
  data4["ADDRESS_HOME_COUNTRY"] = "United States";
  data4["PHONE_HOME_WHOLE_NUMBER"] = "+1 (408) 871-4567";
  profiles.push_back(data4);

  for (size_t i = 0; i < profiles.size(); ++i)
    FillFormAndSubmit("autofill_test_form.html", profiles[i]);

  ASSERT_EQ(4u, personal_data_manager()->GetProfiles().size());

  for (size_t i = 0; i < personal_data_manager()->GetProfiles().size(); ++i) {
    AutofillProfile* profile = personal_data_manager()->GetProfiles()[i];
    std::string expectation;
    std::string name = UTF16ToASCII(profile->GetRawInfo(NAME_FIRST));

    if (name == "Bonnie")
      expectation = "+447624123456";
    else if (name == "John")
      expectation = "+447624123456";
    else if (name == "Jane")
      expectation = "+447624123456";
    else if (name == "Bob")
      expectation = "14088714567";

    EXPECT_EQ(ASCIIToUTF16(expectation),
              profile->GetInfo(PHONE_HOME_WHOLE_NUMBER, ""));
  }
}

// Test profile not aggregated if email found in non-email field.
IN_PROC_BROWSER_TEST_F(AutofillTest, ProfileWithEmailInOtherFieldNotSaved) {
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "bsmith@gmail.com";
  data["ADDRESS_HOME_CITY"] = "San Jose";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "95110";
  data["COMPANY_NAME"] = "Company X";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);

  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());
}

// Test that profiles merge for aggregated data with same address.
// The criterion for when two profiles are expected to be merged is when their
// 'Address Line 1' and 'City' data match. When two profiles are merged, any
// remaining address fields are expected to be overwritten. Any non-address
// fields should accumulate multi-valued data.
// DISABLED: http://crbug.com/281541
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       DISABLED_MergeAggregatedProfilesWithSameAddress) {
  AggregateProfilesIntoAutofillPrefs("dataset_same_address.txt");

  ASSERT_EQ(3u, personal_data_manager()->GetProfiles().size());
}

// Test profiles are not merged without minimum address values.
// Mininum address values needed during aggregation are: address line 1, city,
// state, and zip code.
// Profiles are merged when data for address line 1 and city match.
// DISABLED: http://crbug.com/281541
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       DISABLED_ProfilesNotMergedWhenNoMinAddressData) {
  AggregateProfilesIntoAutofillPrefs("dataset_no_address.txt");

  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());
}

// Test Autofill ability to merge duplicate profiles and throw away junk.
// TODO(isherman): this looks redundant, consider removing.
// DISABLED: http://crbug.com/281541
IN_PROC_BROWSER_TEST_F(AutofillTest,
                       DISABLED_MergeAggregatedDuplicatedProfiles) {
  int num_of_profiles =
      AggregateProfilesIntoAutofillPrefs("dataset_duplicated_profiles.txt");

  ASSERT_GT(num_of_profiles,
            static_cast<int>(personal_data_manager()->GetProfiles().size()));
}

// Accessibility Tests //
class AutofillAccessibilityTest : public AutofillTest {
 protected:
  AutofillAccessibilityTest() {}

  // Returns true if kAutofillAvailable state is present AND  kAutoComplete
  // string attribute is missing; only one should be set at any given time.
  // Returns false otherwise.
  bool AutofillIsAvailable(const ui::AXNodeData& data) {
    if (data.HasState(ax::mojom::State::kAutofillAvailable) &&
        !data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete)) {
      return true;
    }
    return false;
  }

  // Returns true if kAutocomplete string attribute is present AND
  // kAutofillAvailable state is missing; only one should be set at any given
  // time. Returns false otherwise.
  bool AutocompleteIsAvailable(const ui::AXNodeData& data) {
    if (data.HasStringAttribute(ax::mojom::StringAttribute::kAutoComplete) &&
        !data.HasState(ax::mojom::State::kAutofillAvailable)) {
      return true;
    }
    return false;
  }
};

// Test that autofill available state is correctly set on accessibility node.
IN_PROC_BROWSER_TEST_F(AutofillAccessibilityTest, TestAutofillState) {
  // Navigate to url.
  GURL url =
      embedded_test_server()->GetURL("/autofill/duplicate_profiles_test.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Enable accessibility.
  content::EnableAccessibilityForWebContents(web_contents());
  content::AccessibilityNotificationWaiter layout_waiter_one(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLayoutComplete);
  layout_waiter_one.WaitForNotification();

  // Focus target form field.
  const std::string focus_name_first_js =
      "document.getElementById('NAME_FIRST').focus();";
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autofill is not yet available for target form field.
  // Loop while criteria is not met.
  ui::AXNodeData node_data;
  std::string node_name;
  const ax::mojom::Role target_role = ax::mojom::Role::kTextField;
  const std::string target_name = "First Name:";
  while (!(node_data.role == target_role && node_name == target_name &&
           !AutofillIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_FALSE(AutofillIsAvailable(node_data));

  // Fill form and submit.
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  data["ADDRESS_HOME_LINE1"] = "1234 H St.";
  data["ADDRESS_HOME_CITY"] = "Mountain View";
  data["EMAIL_ADDRESS"] = "bsmith@example.com";
  data["ADDRESS_HOME_STATE"] = "CA";
  data["ADDRESS_HOME_ZIP"] = "94043";
  data["ADDRESS_HOME_COUNTRY"] = "United States";
  data["PHONE_HOME_WHOLE_NUMBER"] = "408-871-4567";
  FillFormAndSubmit("duplicate_profiles_test.html", data);
  ASSERT_EQ(1u, personal_data_manager()->GetProfiles().size());

  // Reload page.
  ui_test_utils::NavigateToURL(&params);
  content::AccessibilityNotificationWaiter layout_waiter_two(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLayoutComplete);
  layout_waiter_two.WaitForNotification();

  // Focus target form field.
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autofill is now available for target form field.
  // Loop while criteria is not met.
  while (!(node_data.role == target_role && node_name == target_name &&
           AutofillIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_TRUE(AutofillIsAvailable(node_data));
}

// Test that autocomplete available string attribute is correctly set on
// accessibility node. Test autocomplete in this file since it uses the same
// infrastructure as autofill.
IN_PROC_BROWSER_TEST_F(AutofillAccessibilityTest, TestAutocompleteState) {
  // Navigate to url.
  GURL url =
      embedded_test_server()->GetURL("/autofill/duplicate_profiles_test.html");
  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  ui_test_utils::NavigateToURL(&params);

  // Enable accessibility.
  content::EnableAccessibilityForWebContents(web_contents());
  content::AccessibilityNotificationWaiter layout_waiter_one(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLayoutComplete);
  layout_waiter_one.WaitForNotification();

  // Focus target form field.
  const std::string focus_name_first_js =
      "document.getElementById('NAME_FIRST').focus();";
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autocomplete is not yet available for target form field.
  // Loop while criteria is not met.
  ui::AXNodeData node_data;
  std::string node_name;
  const ax::mojom::Role target_role = ax::mojom::Role::kTextField;
  const std::string target_name = "First Name:";
  while (!(node_data.role == target_role && node_name == target_name &&
           !AutocompleteIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_FALSE(AutocompleteIsAvailable(node_data));

  // Partially fill form. This should not set autofill state, but rather,
  // autocomplete state.
  FormMap data;
  data["NAME_FIRST"] = "Bob";
  data["NAME_LAST"] = "Smith";
  FillFormAndSubmit("duplicate_profiles_test.html", data);
  // Since we didn't fill the entire form, we should not have increased the
  // number of autofill profiles.
  ASSERT_EQ(0u, personal_data_manager()->GetProfiles().size());

  // Reload page.
  ui_test_utils::NavigateToURL(&params);
  content::AccessibilityNotificationWaiter layout_waiter_two(
      web_contents(), ui::kAXModeComplete, ax::mojom::Event::kLayoutComplete);
  layout_waiter_two.WaitForNotification();

  // Focus target form field.
  ASSERT_TRUE(content::ExecuteScript(web_contents(), focus_name_first_js));

  // Assert that autocomplete is now available for target form field.
  // Loop while criteria is not met.
  while (!(node_data.role == target_role && node_name == target_name &&
           AutocompleteIsAvailable(node_data))) {
    content::WaitForAccessibilityTreeToChange(web_contents());
    node_data = content::GetFocusedAccessibilityNodeInfo(web_contents());
    node_name = node_data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  }
  // Sanity check.
  ASSERT_TRUE(AutocompleteIsAvailable(node_data));
}

}  // namespace autofill
