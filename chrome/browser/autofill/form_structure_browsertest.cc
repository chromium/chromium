// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/heuristic_source.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/data_driven_testing/data_driven_test.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

namespace autofill {
namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

const base::FilePath::CharType kFeatureName[] = FILE_PATH_LITERAL("autofill");
const base::FilePath::CharType kTestName[] = FILE_PATH_LITERAL("heuristics");

// To disable a data driven test, please add the name of the test file
// (i.e., FILE_PATH_LITERAL("NNN_some_site.html")) to the initializer_list given
// to the failing_test_names constructor.
const auto& GetFailingTestNames() {
  static std::set<base::FilePath::StringType> failing_test_names{};
  return failing_test_names;
}

const base::FilePath& GetTestDataDir() {
  static base::NoDestructor<base::FilePath> dir([]() {
    base::FilePath dir;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &dir);
    dir = dir.AppendASCII("components").AppendASCII("test").AppendASCII("data");
    return dir;
  }());
  return *dir;
}

const base::FilePath GetInputDir() {
  static base::FilePath input_dir = GetTestDataDir()
                                        .Append(kFeatureName)
                                        .Append(kTestName)
                                        .AppendASCII("input");
  return input_dir;
}

std::vector<base::FilePath> GetTestFiles() {
  base::FileEnumerator input_files(GetInputDir(), false,
                                   base::FileEnumerator::FILES);
  std::vector<base::FilePath> files;
  for (base::FilePath input_file = input_files.Next(); !input_file.empty();
       input_file = input_files.Next()) {
    files.push_back(input_file);
  }
  std::sort(files.begin(), files.end());

#if BUILDFLAG(IS_MAC)
  base::apple::ClearAmIBundledCache();
#endif  // BUILDFLAG(IS_MAC)

  return files;
}

std::string FormStructuresToString(
    const std::map<FormGlobalId, std::unique_ptr<FormStructure>>& forms) {
  std::vector<std::string> string_forms;
  string_forms.reserve(forms.size());
  // The forms are sorted by their global ID, which should make the order
  // deterministic.
  for (const auto& [form_id, form_structure] : forms) {
    std::string string_form;
    std::map<std::string, int> section_to_index;
    for (const auto& field : *form_structure) {
      std::string section = field->section().ToString();
      if (field->section().is_from_fieldidentifier()) {
        // Normalize the section by replacing the unique but platform-dependent
        // integers in `field->section` with consecutive unique integers.
        // The section string is of the form "fieldname_id1_id2", where id1, id2
        // are platform-dependent and thus need to be substituted.
        size_t last_underscore = section.find_last_of('_');
        size_t second_last_underscore =
            section.find_last_of('_', last_underscore - 1);
        int new_section_index = static_cast<int>(section_to_index.size() + 1);
        int section_index =
            section_to_index.insert(std::make_pair(section, new_section_index))
                .first->second;
        if (second_last_underscore != std::string::npos) {
          section = base::StringPrintf(
              "%s%d", section.substr(0, second_last_underscore + 1).c_str(),
              section_index);
        }
      }
      string_form += base::JoinString(
          {field->Type().ToStringView(), base::UTF16ToUTF8(field->name()),
           base::UTF16ToUTF8(field->label()),
           base::UTF16ToUTF8(field->value(ValueSemantics::kCurrent)), section},
          " | ");
      string_form.push_back('\n');
    }
    string_forms.push_back(string_form);
  }
  sort(string_forms.begin(), string_forms.end());
  return base::JoinString(string_forms, "\n");
}

// A data-driven test for verifying Autofill heuristics. Each input is an HTML
// file that contains one or more forms. The corresponding output file lists the
// heuristically detected type for each field.
class FormStructureBrowserTest
    : public InProcessBrowserTest,
      public testing::DataDrivenTest,
      public testing::WithParamInterface<base::FilePath> {
 public:
  FormStructureBrowserTest(const FormStructureBrowserTest&) = delete;
  FormStructureBrowserTest& operator=(const FormStructureBrowserTest&) = delete;

 protected:
  FormStructureBrowserTest();
  ~FormStructureBrowserTest() override;

  // InProcessBrowserTest
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // BrowserTestBase
  void SetUpOnMainThread() override;

  // DataDrivenTest:
  void GenerateResults(const std::string& input, std::string* output) override;

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  class TestAutofillManager : public BrowserAutofillManager {
   public:
    explicit TestAutofillManager(ContentAutofillDriver* driver)
        : BrowserAutofillManager(driver, "en-US") {}

    TestAutofillManagerWaiter& waiter() { return waiter_; }

   private:
    TestAutofillManagerWaiter waiter_{*this,
                                      {AutofillManagerEvent::kFormsSeen}};
  };

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  // The response content to be returned by the embedded test server. Note that
  // this is populated in the main thread as a part of the setup in the
  // GenerateResults method but it is consumed later in the IO thread by the
  // embedded test server to generate the response.
  std::string html_content_;

  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
  base::test::ScopedFeatureList feature_list_;
};

FormStructureBrowserTest::FormStructureBrowserTest()
    : ::testing::DataDrivenTest(GetTestDataDir(), kFeatureName, kTestName) {
  feature_list_.InitWithFeatures(
      // Enabled
      {
          features::kAutofillPageLanguageDetection,
          features::kAutofillFixValueSemantics,
          // TODO(crbug.com/40741721): Remove once shared labels are launched.
          features::kAutofillEnableSupportForParsingWithSharedLabels,
          // TODO(crbug.com/40230674): Remove once launched.
          features::kAutofillParseVcnCardOnFileStandaloneCvcFields,
          // TODO(crbug.com/40220393): Remove once launched.
          features::kAutofillEnableSupportForPhoneNumberTrunkTypes,
          features::kAutofillInferCountryCallingCode,
          // TODO(crbug.com/40266396): Remove once launched.
          features::kAutofillEnableExpirationDateImprovements,
      },
      // Disabled
      {// TODO(crbug.com/40220393): Remove once launched.
       // This feature is part of the AutofillRefinedPhoneNumberTypes rollout.
       // As it is not supported on iOS yet, it is disabled.
       features::kAutofillConsiderPhoneNumberSeparatorsValidLabels,
       // TODO(crbug.com/40222716): Remove once launched. This feature is
       // disabled since it is not supported on iOS.
       features::kAutofillAlwaysParsePlaceholders,
       // TODO(crbug.com/1493145): Remove when/if launched. This feature changes
       // default parsing behavior, so must be disabled to avoid
       // fieldtrial_testing_config interference.
       features::kAutofillEnableEmailHeuristicOnlyAddressForms});
}

FormStructureBrowserTest::~FormStructureBrowserTest() = default;

void FormStructureBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Suppress most output logs because we can't really control the output for
  // arbitrary test sites.
  command_line->AppendSwitchASCII(switches::kLoggingLevel, "2");
  command_line->AppendSwitchASCII(
      variations::switches::kVariationsOverrideCountry, "us");
  // SelectParserRelaxation affects the results from the test data because the
  // test data has unclosed <select> tags. Since SelectParserRelaxation is not
  // enabled by default, we are disabling it for this test.
  command_line->AppendSwitchASCII("disable-blink-features",
                                  "SelectParserRelaxation");
}

void FormStructureBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &FormStructureBrowserTest::HandleRequest, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
}

void FormStructureBrowserTest::GenerateResults(const std::string& input,
                                               std::string* output) {
  // Cache the content to be returned by the embedded test server. This data
  // is readonly after this point.
  html_content_.clear();
  html_content_.reserve(input.length());
  for (const char c : input) {
    // Strip `\n`, `\t`, `\r` from |html| to match old `data:` URL behavior.
    // TODO(crbug.com/40317270): the tests expect weird concatenation behavior
    // based
    //   legacy data URL behavior. Fix this so the the tests better represent
    //   the parsing being done in the wild.
    if (c != '\r' && c != '\n' && c != '\t')
      html_content_.push_back(c);
  }

  ASSERT_NO_FATAL_FAILURE(ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test.html"))));

  // Dump the form fields (and their inferred field types).
  TestAutofillManager* autofill_manager =
      autofill_manager_injector_[web_contents()];
  ASSERT_TRUE(autofill_manager->waiter().Wait(1));
  *output = FormStructuresToString(autofill_manager->form_structures());
}

std::unique_ptr<HttpResponse> FormStructureBrowserTest::HandleRequest(
    const HttpRequest& request) {
  auto response = std::make_unique<BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(html_content_);
  response->set_content_type("text/html; charset=utf-8");
  return std::move(response);
}

IN_PROC_BROWSER_TEST_P(FormStructureBrowserTest, DataDrivenHeuristics) {
#if !BUILDFLAG(USE_INTERNAL_AUTOFILL_PATTERNS)
  if (GetActiveHeuristicSource() != HeuristicSource::kLegacyRegexes) {
    GTEST_SKIP() << "DataDrivenHeuristics tests are only supported with legacy "
                    "parsing patterns";
  }
#endif
  // Prints the path of the test to be executed.
  LOG(INFO) << GetParam().MaybeAsASCII();
  bool is_expected_to_pass =
      !base::Contains(GetFailingTestNames(), GetParam().BaseName().value());
  RunOneDataDrivenTest(GetParam(), GetOutputDirectory(), is_expected_to_pass);
}

INSTANTIATE_TEST_SUITE_P(AllForms,
                         FormStructureBrowserTest,
                         testing::ValuesIn(GetTestFiles()));

}  // namespace
}  // namespace autofill
