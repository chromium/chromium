// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/font_pref_change_notifier.h"
#include "base/functional/bind.h"
#include "chrome/common/pref_names_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void AppendString(std::vector<std::string>* vect, const std::string& str) {
  vect->push_back(str);
}

}  // namespace

TEST(FontPrefChangeNotifier, Registrars) {
  std::string prefix((pref_names_util::kWebKitFontPrefPrefix));

  // Registrar 0 will outlive the notifier.
  std::vector<std::string> fonts0;
  FontPrefChangeNotifier::Registrar reg0;

  std::unique_ptr<TestingPrefServiceSimple> service =
      std::make_unique<TestingPrefServiceSimple>();
  PrefRegistrySimple* pref_registry = service->registry();

  std::string font1 = prefix + "font1";
  pref_registry->RegisterStringPref(font1, std::string());
  std::string font2 = prefix + "font2";
  pref_registry->RegisterStringPref(font2, std::string());
  std::string font3 = prefix + "font3";
  pref_registry->RegisterStringPref(font3, std::string());

  std::unique_ptr<FontPrefChangeNotifier> notifier =
      std::make_unique<FontPrefChangeNotifier>(service.get());
  reg0.Register(notifier.get(),
                base::BindRepeating(&AppendString, base::Unretained(&fonts0)));

  // Registrar 1 will be manually unregistered.
  std::vector<std::string> fonts1;
  FontPrefChangeNotifier::Registrar reg1;
  reg1.Register(notifier.get(),
                base::BindRepeating(&AppendString, base::Unretained(&fonts1)));

  // Registrar 2 will automatically unregister itself when it goes out of scope.
  std::vector<std::string> fonts2;
  {
    FontPrefChangeNotifier::Registrar reg2;
    reg2.Register(
        notifier.get(),
        base::BindRepeating(&AppendString, base::Unretained(&fonts2)));

    // All lists should get the font.
    service->SetString(font1, "1");
    EXPECT_EQ(1u, fonts0.size());
    EXPECT_EQ(font1, fonts0.back());
    EXPECT_EQ(1u, fonts1.size());
    EXPECT_EQ(font1, fonts1.back());
    EXPECT_EQ(1u, fonts2.size());
    EXPECT_EQ(font1, fonts2.back());
  }

  // Now that Regsitrar 2 is gone, only 0 and 1 should get changes.
  service->SetString(font2, "2");
  EXPECT_EQ(2u, fonts0.size());
  EXPECT_EQ(font2, fonts0.back());
  EXPECT_EQ(2u, fonts1.size());
  EXPECT_EQ(font2, fonts1.back());
  EXPECT_EQ(1u, fonts2.size());

  // Manually unregister Registrar 1.
  reg1.Unregister();
  EXPECT_FALSE(reg1.is_registered());

  // Only Registrar 0 should see changes now.
  service->SetString(font3, "3");
  EXPECT_EQ(3u, fonts0.size());
  EXPECT_EQ(font3, fonts0.back());
  EXPECT_EQ(2u, fonts1.size());
  EXPECT_EQ(1u, fonts2.size());
  EXPECT_EQ(font3, fonts0.back());

  notifier.reset();

  // Registrar 0 should have been automatically unregistered.
  EXPECT_FALSE(reg0.is_registered());
}
