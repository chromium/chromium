// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_method_manager/input_method_prefs.h"

#include <optional>

#include "base/strings/stringprintf.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"

namespace arc {

class InputMethodPrefsTest : public testing::Test {
 public:
  InputMethodPrefsTest() = default;
  ~InputMethodPrefsTest() override = default;

  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

TEST_F(InputMethodPrefsTest, Constructor) {
  InputMethodPrefs prefs(profile());
}

TEST_F(InputMethodPrefsTest, GetEnabledImes) {
  namespace aeiu = ::ash::extension_ime_util;
  using crx_file::id_util::GenerateId;

  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id =
      aeiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");

  profile()->GetPrefs()->SetString(
      prefs::kLanguageEnabledImes,
      base::StringPrintf("%s,%s", component_extension_ime_id.c_str(),
                         arc_ime_id.c_str()));

  InputMethodPrefs prefs(profile());
  const std::set<std::string> imes = prefs.GetEnabledImes();
  EXPECT_EQ(2u, imes.size());
  EXPECT_TRUE(imes.count(component_extension_ime_id) > 0);
  EXPECT_TRUE(imes.count(arc_ime_id) > 0);
}

TEST_F(InputMethodPrefsTest, UpdateEnabledImes) {
  namespace aeiu = ::ash::extension_ime_util;
  using ::ash::input_method::InputMethodDescriptor;
  using crx_file::id_util::GenerateId;

  const std::string component_extension_ime_id =
      aeiu::GetComponentInputMethodID(
          GenerateId("test.component.extension.ime"), "us");
  const std::string arc_ime_id1 =
      aeiu::GetArcInputMethodID(GenerateId("test.arc.ime"), "us");
  const std::string arc_ime_id2 =
      aeiu::GetArcInputMethodID(GenerateId("test.arc.ime2"), "us");

  PrefService* pref_service = profile()->GetPrefs();

  // Enabled one component extension IME.
  pref_service->SetString(prefs::kLanguageEnabledImes,
                          component_extension_ime_id);

  InputMethodPrefs prefs(profile());
  InputMethodDescriptor arc_ime_descriptor1(
      arc_ime_id1, "", "", {}, {}, false, GURL(), GURL(),
      /*handwriting_language=*/std::nullopt);
  InputMethodDescriptor arc_ime_descriptor2(
      arc_ime_id2, "", "", {}, {}, false, GURL(), GURL(),
      /*handwriting_language=*/std::nullopt);

  {
    prefs.UpdateEnabledImes({arc_ime_descriptor1});
    const std::set<std::string> imes = prefs.GetEnabledImes();
    EXPECT_EQ(2u, imes.size());
    EXPECT_TRUE(imes.count(component_extension_ime_id) > 0);
    EXPECT_TRUE(imes.count(arc_ime_id1) > 0);
  }

  // Enable both IMEs and set current/previous IME.
  prefs.UpdateEnabledImes({arc_ime_descriptor1, arc_ime_descriptor2});
  pref_service->SetString(prefs::kLanguageCurrentInputMethod, arc_ime_id1);
  pref_service->SetString(prefs::kLanguagePreviousInputMethod, arc_ime_id2);

  {
    const std::set<std::string> imes = prefs.GetEnabledImes();
    EXPECT_EQ(3u, imes.size());
    EXPECT_TRUE(imes.count(component_extension_ime_id) > 0);
    EXPECT_TRUE(imes.count(arc_ime_id1) > 0);
    EXPECT_TRUE(imes.count(arc_ime_id2) > 0);
  }

  // Disable both ARC IMEs. It should reset current/previous IMEs.
  {
    prefs.UpdateEnabledImes({});
    const std::set<std::string> imes = prefs.GetEnabledImes();
    EXPECT_EQ(1u, imes.size());
    EXPECT_TRUE(imes.count(component_extension_ime_id) > 0);
    EXPECT_EQ("", pref_service->GetString(prefs::kLanguageCurrentInputMethod));
    EXPECT_EQ("", pref_service->GetString(prefs::kLanguagePreviousInputMethod));
  }
}

}  // namespace arc
