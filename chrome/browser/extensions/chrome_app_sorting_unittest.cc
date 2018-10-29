// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_app_sorting.h"

#include <memory>

#include "chrome/browser/extensions/extension_prefs_unittest.h"
#include "components/crx_file/id_util.h"
#include "components/sync/model/string_ordinal.h"
#include "extensions/common/constants.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace keys = manifest_keys;

class ChromeAppSortingAppLocation : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension_ = prefs_.AddExtension("not_an_app");
    // Non-apps should not have any app launch ordinal or page ordinal.
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());
  }

  void Verify() override {
    EXPECT_FALSE(
        app_sorting()->GetAppLaunchOrdinal(extension_->id()).IsValid());
    EXPECT_FALSE(app_sorting()->GetPageOrdinal(extension_->id()).IsValid());
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ChromeAppSortingAppLocation, ChromeAppSortingAppLocation) {}

class ChromeAppSortingAppLaunchOrdinal : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    // No extensions yet.
    syncer::StringOrdinal page = syncer::StringOrdinal::CreateInitialOrdinal();
    EXPECT_TRUE(syncer::StringOrdinal::CreateInitialOrdinal().Equals(
        app_sorting()->CreateNextAppLaunchOrdinal(page)));

    extension_ = prefs_.AddApp("on_extension_installed");
    EXPECT_FALSE(prefs()->IsExtensionDisabled(extension_->id()));
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());
  }

  void Verify() override {
    syncer::StringOrdinal launch_ordinal =
        app_sorting()->GetAppLaunchOrdinal(extension_->id());
    syncer::StringOrdinal page_ordinal =
        syncer::StringOrdinal::CreateInitialOrdinal();

    // Extension should have been assigned a valid StringOrdinal.
    EXPECT_TRUE(launch_ordinal.IsValid());
    EXPECT_TRUE(launch_ordinal.LessThan(
        app_sorting()->CreateNextAppLaunchOrdinal(page_ordinal)));
    // Set a new launch ordinal of and verify it comes after.
    app_sorting()->SetAppLaunchOrdinal(
        extension_->id(),
        app_sorting()->CreateNextAppLaunchOrdinal(page_ordinal));
    syncer::StringOrdinal new_launch_ordinal =
        app_sorting()->GetAppLaunchOrdinal(extension_->id());
    EXPECT_TRUE(launch_ordinal.LessThan(new_launch_ordinal));

    // This extension doesn't exist, so it should return an invalid
    // StringOrdinal.
    syncer::StringOrdinal invalid_app_launch_ordinal =
        app_sorting()->GetAppLaunchOrdinal("foo");
    EXPECT_FALSE(invalid_app_launch_ordinal.IsValid());
    EXPECT_EQ(-1, app_sorting()->PageStringOrdinalAsInteger(
        invalid_app_launch_ordinal));

    // The second page doesn't have any apps so its next launch ordinal should
    // be the first launch ordinal.
    syncer::StringOrdinal next_page = page_ordinal.CreateAfter();
    syncer::StringOrdinal next_page_app_launch_ordinal =
        app_sorting()->CreateNextAppLaunchOrdinal(next_page);
    EXPECT_TRUE(next_page_app_launch_ordinal.Equals(
        app_sorting()->CreateFirstAppLaunchOrdinal(next_page)));
  }

 private:
  scoped_refptr<Extension> extension_;
};
TEST_F(ChromeAppSortingAppLaunchOrdinal, ChromeAppSortingAppLaunchOrdinal) {}

class ChromeAppSortingPageOrdinal : public ExtensionPrefsTest {
 public:
  void Initialize() override {
    extension_ = prefs_.AddApp("page_ordinal");
    // Install with a page preference.
    first_page_ = syncer::StringOrdinal::CreateInitialOrdinal();
    prefs()->OnExtensionInstalled(extension_.get(),
                                  Extension::ENABLED,
                                  first_page_,
                                  std::string());
    EXPECT_TRUE(first_page_.Equals(
        app_sorting()->GetPageOrdinal(extension_->id())));
    EXPECT_EQ(0, app_sorting()->PageStringOrdinalAsInteger(first_page_));

    scoped_refptr<Extension> extension2 = prefs_.AddApp("page_ordinal_2");
    // Install without any page preference.
    prefs()->OnExtensionInstalled(extension2.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());
    EXPECT_TRUE(first_page_.Equals(
        app_sorting()->GetPageOrdinal(extension2->id())));
  }
  void Verify() override {
    // Set the page ordinal.
    syncer::StringOrdinal new_page = first_page_.CreateAfter();
    app_sorting()->SetPageOrdinal(extension_->id(), new_page);
    // Verify the page ordinal.
    EXPECT_TRUE(
        new_page.Equals(app_sorting()->GetPageOrdinal(extension_->id())));
    EXPECT_EQ(1, app_sorting()->PageStringOrdinalAsInteger(new_page));

    // This extension doesn't exist, so it should return an invalid
    // StringOrdinal.
    EXPECT_FALSE(app_sorting()->GetPageOrdinal("foo").IsValid());
  }

 private:
  syncer::StringOrdinal first_page_;
  scoped_refptr<Extension> extension_;
};
TEST_F(ChromeAppSortingPageOrdinal, ChromeAppSortingPageOrdinal) {}

// Ensure that ChromeAppSorting is able to properly initialize off a set
// of old page and app launch indices and properly convert them.
class ChromeAppSortingInitialize : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingInitialize() {}
  ~ChromeAppSortingInitialize() override {}

  void Initialize() override {
    // A preference determining the order of which the apps appear on the NTP.
    const char kPrefAppLaunchIndexDeprecated[] = "app_launcher_index";
    // A preference determining the page on which an app appears in the NTP.
    const char kPrefPageIndexDeprecated[] = "page_index";

    // Setup the deprecated preferences.
    prefs()->UpdateExtensionPref(extension1()->id(),
                                 kPrefAppLaunchIndexDeprecated,
                                 std::make_unique<base::Value>(0));
    prefs()->UpdateExtensionPref(extension1()->id(), kPrefPageIndexDeprecated,
                                 std::make_unique<base::Value>(0));

    prefs()->UpdateExtensionPref(extension2()->id(),
                                 kPrefAppLaunchIndexDeprecated,
                                 std::make_unique<base::Value>(1));
    prefs()->UpdateExtensionPref(extension2()->id(), kPrefPageIndexDeprecated,
                                 std::make_unique<base::Value>(0));

    prefs()->UpdateExtensionPref(extension3()->id(),
                                 kPrefAppLaunchIndexDeprecated,
                                 std::make_unique<base::Value>(0));
    prefs()->UpdateExtensionPref(extension3()->id(), kPrefPageIndexDeprecated,
                                 std::make_unique<base::Value>(1));

    // We insert the ids in reverse order so that we have to deal with the
    // element on the 2nd page before the 1st page is seen.
    ExtensionIdList ids;
    ids.push_back(extension3()->id());
    ids.push_back(extension2()->id());
    ids.push_back(extension1()->id());

    app_sorting()->MigrateAppIndex(ids);
  }
  void Verify() override {
    syncer::StringOrdinal first_ordinal =
        syncer::StringOrdinal::CreateInitialOrdinal();

    EXPECT_TRUE(first_ordinal.Equals(
        app_sorting()->GetAppLaunchOrdinal(extension1()->id())));
    EXPECT_TRUE(first_ordinal.LessThan(
        app_sorting()->GetAppLaunchOrdinal(extension2()->id())));
    EXPECT_TRUE(first_ordinal.Equals(
        app_sorting()->GetAppLaunchOrdinal(extension3()->id())));

    EXPECT_TRUE(first_ordinal.Equals(
        app_sorting()->GetPageOrdinal(extension1()->id())));
    EXPECT_TRUE(first_ordinal.Equals(
        app_sorting()->GetPageOrdinal(extension2()->id())));
    EXPECT_TRUE(first_ordinal.LessThan(
        app_sorting()->GetPageOrdinal(extension3()->id())));
  }
};
TEST_F(ChromeAppSortingInitialize, ChromeAppSortingInitialize) {}

// Make sure that initialization still works when no extensions are present
// (i.e. make sure that the web store icon is still loaded into the map).
class ChromeAppSortingInitializeWithNoApps : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingInitializeWithNoApps() {}
  ~ChromeAppSortingInitializeWithNoApps() override {}

  void Initialize() override {
    // Make sure that the web store has valid ordinals.
    syncer::StringOrdinal initial_ordinal =
        syncer::StringOrdinal::CreateInitialOrdinal();
    app_sorting()->SetPageOrdinal(extensions::kWebStoreAppId,
                                  initial_ordinal);
    app_sorting()->SetAppLaunchOrdinal(extensions::kWebStoreAppId,
                                       initial_ordinal);
  }
  void Verify() override {
    syncer::StringOrdinal page =
        app_sorting()->GetPageOrdinal(extensions::kWebStoreAppId);
    EXPECT_TRUE(page.IsValid());

    auto page_it = app_sorting()->ntp_ordinal_map_.find(page);
    EXPECT_TRUE(page_it != app_sorting()->ntp_ordinal_map_.end());

    syncer::StringOrdinal app_launch =
        app_sorting()->GetPageOrdinal(extensions::kWebStoreAppId);
    EXPECT_TRUE(app_launch.IsValid());

    auto app_launch_it = page_it->second.find(app_launch);
    EXPECT_TRUE(app_launch_it != page_it->second.end());
  }
};
TEST_F(ChromeAppSortingInitializeWithNoApps,
       ChromeAppSortingInitializeWithNoApps) {}

// Tests the application index to ordinal migration code for values that
// shouldn't be converted. This should be removed when the migrate code
// is taken out.
// http://crbug.com/107376
class ChromeAppSortingMigrateAppIndexInvalid
    : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingMigrateAppIndexInvalid() {}
  ~ChromeAppSortingMigrateAppIndexInvalid() override {}

  void Initialize() override {
    // A preference determining the order of which the apps appear on the NTP.
    const char kPrefAppLaunchIndexDeprecated[] = "app_launcher_index";
    // A preference determining the page on which an app appears in the NTP.
    const char kPrefPageIndexDeprecated[] = "page_index";

    // Setup the deprecated preference.
    prefs()->UpdateExtensionPref(extension1()->id(),
                                 kPrefAppLaunchIndexDeprecated,
                                 std::make_unique<base::Value>(0));
    prefs()->UpdateExtensionPref(extension1()->id(), kPrefPageIndexDeprecated,
                                 std::make_unique<base::Value>(-1));
  }
  void Verify() override {
    // Make sure that the invalid page_index wasn't converted over.
    EXPECT_FALSE(app_sorting()->GetAppLaunchOrdinal(
        extension1()->id()).IsValid());
  }
};
TEST_F(ChromeAppSortingMigrateAppIndexInvalid,
       ChromeAppSortingMigrateAppIndexInvalid) {}

class ChromeAppSortingFixNTPCollisionsAllCollide
    : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingFixNTPCollisionsAllCollide() {}
  ~ChromeAppSortingFixNTPCollisionsAllCollide() override {}

  void Initialize() override {
    repeated_ordinal_ = syncer::StringOrdinal::CreateInitialOrdinal();

    app_sorting()->SetAppLaunchOrdinal(extension1()->id(),
                                       repeated_ordinal_);
    app_sorting()->SetPageOrdinal(extension1()->id(), repeated_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension2()->id(), repeated_ordinal_);
    app_sorting()->SetPageOrdinal(extension2()->id(), repeated_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension3()->id(), repeated_ordinal_);
    app_sorting()->SetPageOrdinal(extension3()->id(), repeated_ordinal_);

    app_sorting()->FixNTPOrdinalCollisions();
  }
  void Verify() override {
    syncer::StringOrdinal extension1_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension1()->id());
    syncer::StringOrdinal extension2_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension2()->id());
    syncer::StringOrdinal extension3_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension3()->id());

    // The overlapping extensions should have be adjusted so that they are
    // sorted by their id.
    EXPECT_EQ(extension1()->id() < extension2()->id(),
              extension1_app_launch.LessThan(extension2_app_launch));
    EXPECT_EQ(extension1()->id() < extension3()->id(),
              extension1_app_launch.LessThan(extension3_app_launch));
    EXPECT_EQ(extension2()->id() < extension3()->id(),
              extension2_app_launch.LessThan(extension3_app_launch));

    // The page ordinal should be unchanged.
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension1()->id()).Equals(
        repeated_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension2()->id()).Equals(
        repeated_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension3()->id()).Equals(
        repeated_ordinal_));
  }

 private:
  syncer::StringOrdinal repeated_ordinal_;
};
TEST_F(ChromeAppSortingFixNTPCollisionsAllCollide,
       ChromeAppSortingFixNTPCollisionsAllCollide) {}

class ChromeAppSortingFixNTPCollisionsSomeCollideAtStart
    : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingFixNTPCollisionsSomeCollideAtStart() {}
  ~ChromeAppSortingFixNTPCollisionsSomeCollideAtStart() override {}

  void Initialize() override {
    first_ordinal_ = syncer::StringOrdinal::CreateInitialOrdinal();
    syncer::StringOrdinal second_ordinal = first_ordinal_.CreateAfter();

    // Have the first two extension in the same position, with a third
    // (non-colliding) extension after.

    app_sorting()->SetAppLaunchOrdinal(extension1()->id(), first_ordinal_);
    app_sorting()->SetPageOrdinal(extension1()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension2()->id(), first_ordinal_);
    app_sorting()->SetPageOrdinal(extension2()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension3()->id(), second_ordinal);
    app_sorting()->SetPageOrdinal(extension3()->id(), first_ordinal_);

    app_sorting()->FixNTPOrdinalCollisions();
  }
  void Verify() override {
    syncer::StringOrdinal extension1_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension1()->id());
    syncer::StringOrdinal extension2_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension2()->id());
    syncer::StringOrdinal extension3_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension3()->id());

    // The overlapping extensions should have be adjusted so that they are
    // sorted by their id, but they both should be before ext3, which wasn't
    // overlapping.
    EXPECT_EQ(extension1()->id() < extension2()->id(),
              extension1_app_launch.LessThan(extension2_app_launch));
    EXPECT_TRUE(extension1_app_launch.LessThan(extension3_app_launch));
    EXPECT_TRUE(extension2_app_launch.LessThan(extension3_app_launch));

    // The page ordinal should be unchanged.
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension1()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension2()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension3()->id()).Equals(
        first_ordinal_));
  }

 private:
  syncer::StringOrdinal first_ordinal_;
};
TEST_F(ChromeAppSortingFixNTPCollisionsSomeCollideAtStart,
       ChromeAppSortingFixNTPCollisionsSomeCollideAtStart) {}

class ChromeAppSortingFixNTPCollisionsSomeCollideAtEnd
    : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingFixNTPCollisionsSomeCollideAtEnd() {}
  ~ChromeAppSortingFixNTPCollisionsSomeCollideAtEnd() override {}

  void Initialize() override {
    first_ordinal_ = syncer::StringOrdinal::CreateInitialOrdinal();
    syncer::StringOrdinal second_ordinal = first_ordinal_.CreateAfter();

    // Have the first extension in a non-colliding position, followed by two
    // two extension in the same position.

    app_sorting()->SetAppLaunchOrdinal(extension1()->id(), first_ordinal_);
    app_sorting()->SetPageOrdinal(extension1()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension2()->id(), second_ordinal);
    app_sorting()->SetPageOrdinal(extension2()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension3()->id(), second_ordinal);
    app_sorting()->SetPageOrdinal(extension3()->id(), first_ordinal_);

    app_sorting()->FixNTPOrdinalCollisions();
  }
  void Verify() override {
    syncer::StringOrdinal extension1_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension1()->id());
    syncer::StringOrdinal extension2_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension2()->id());
    syncer::StringOrdinal extension3_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension3()->id());

    // The overlapping extensions should have be adjusted so that they are
    // sorted by their id, but they both should be after ext1, which wasn't
    // overlapping.
    EXPECT_TRUE(extension1_app_launch.LessThan(extension2_app_launch));
    EXPECT_TRUE(extension1_app_launch.LessThan(extension3_app_launch));
    EXPECT_EQ(extension2()->id() < extension3()->id(),
              extension2_app_launch.LessThan(extension3_app_launch));

    // The page ordinal should be unchanged.
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension1()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension2()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension3()->id()).Equals(
        first_ordinal_));
  }

 private:
  syncer::StringOrdinal first_ordinal_;
};
TEST_F(ChromeAppSortingFixNTPCollisionsSomeCollideAtEnd,
       ChromeAppSortingFixNTPCollisionsSomeCollideAtEnd) {}

class ChromeAppSortingFixNTPCollisionsTwoCollisions
    : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingFixNTPCollisionsTwoCollisions() {}
  ~ChromeAppSortingFixNTPCollisionsTwoCollisions() override {}

  void Initialize() override {
    first_ordinal_ = syncer::StringOrdinal::CreateInitialOrdinal();
    syncer::StringOrdinal second_ordinal = first_ordinal_.CreateAfter();

    // Have two extensions colliding, followed by two more colliding extensions.
    app_sorting()->SetAppLaunchOrdinal(extension1()->id(), first_ordinal_);
    app_sorting()->SetPageOrdinal(extension1()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension2()->id(), first_ordinal_);
    app_sorting()->SetPageOrdinal(extension2()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension3()->id(), second_ordinal);
    app_sorting()->SetPageOrdinal(extension3()->id(), first_ordinal_);

    app_sorting()->SetAppLaunchOrdinal(extension4()->id(), second_ordinal);
    app_sorting()->SetPageOrdinal(extension4()->id(), first_ordinal_);

    app_sorting()->FixNTPOrdinalCollisions();
  }
  void Verify() override {
    syncer::StringOrdinal extension1_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension1()->id());
    syncer::StringOrdinal extension2_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension2()->id());
    syncer::StringOrdinal extension3_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension3()->id());
    syncer::StringOrdinal extension4_app_launch =
        app_sorting()->GetAppLaunchOrdinal(extension4()->id());

    // The overlapping extensions should have be adjusted so that they are
    // sorted by their id, with |ext1| and |ext2| appearing before |ext3| and
    // |ext4|.
    EXPECT_TRUE(extension1_app_launch.LessThan(extension3_app_launch));
    EXPECT_TRUE(extension1_app_launch.LessThan(extension4_app_launch));
    EXPECT_TRUE(extension2_app_launch.LessThan(extension3_app_launch));
    EXPECT_TRUE(extension2_app_launch.LessThan(extension4_app_launch));

    EXPECT_EQ(extension1()->id() < extension2()->id(),
              extension1_app_launch.LessThan(extension2_app_launch));
    EXPECT_EQ(extension3()->id() < extension4()->id(),
              extension3_app_launch.LessThan(extension4_app_launch));

    // The page ordinal should be unchanged.
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension1()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension2()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension3()->id()).Equals(
        first_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension4()->id()).Equals(
        first_ordinal_));
  }

 private:
  syncer::StringOrdinal first_ordinal_;
};
TEST_F(ChromeAppSortingFixNTPCollisionsTwoCollisions,
       ChromeAppSortingFixNTPCollisionsTwoCollisions) {}

class ChromeAppSortingEnsureValidOrdinals
    : public PrefsPrepopulatedTestBase {
 public :
  ChromeAppSortingEnsureValidOrdinals() {}
  ~ChromeAppSortingEnsureValidOrdinals() override {}

  void Initialize() override {}
  void Verify() override {
    // Give ext1 invalid ordinals and then check that EnsureValidOrdinals fixes
    // them.
    app_sorting()->SetAppLaunchOrdinal(extension1()->id(),
                                     syncer::StringOrdinal());
    app_sorting()->SetPageOrdinal(extension1()->id(), syncer::StringOrdinal());

    app_sorting()->EnsureValidOrdinals(extension1()->id(),
                                     syncer::StringOrdinal());

    EXPECT_TRUE(
        app_sorting()->GetAppLaunchOrdinal(extension1()->id()).IsValid());
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(extension1()->id()).IsValid());
  }
};
TEST_F(ChromeAppSortingEnsureValidOrdinals,
       ChromeAppSortingEnsureValidOrdinals) {}

class ChromeAppSortingPageOrdinalMapping : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingPageOrdinalMapping() {}
  ~ChromeAppSortingPageOrdinalMapping() override {}

  void Initialize() override {}
  void Verify() override {
    std::string ext_1 = "ext_1";
    std::string ext_2 = "ext_2";

    syncer::StringOrdinal first_ordinal =
        syncer::StringOrdinal::CreateInitialOrdinal();

    // Ensure attempting to removing a mapping with an invalid page doesn't
    // modify the map.
    EXPECT_TRUE(app_sorting()->ntp_ordinal_map_.empty());
    app_sorting()->RemoveOrdinalMapping(
        ext_1, first_ordinal, first_ordinal);
    EXPECT_TRUE(app_sorting()->ntp_ordinal_map_.empty());

    // Add new mappings.
    app_sorting()->AddOrdinalMapping(ext_1, first_ordinal, first_ordinal);
    app_sorting()->AddOrdinalMapping(ext_2, first_ordinal, first_ordinal);

    EXPECT_EQ(1U, app_sorting()->ntp_ordinal_map_.size());
    EXPECT_EQ(2U, app_sorting()->ntp_ordinal_map_[first_ordinal].size());

    auto it =
        app_sorting()->ntp_ordinal_map_[first_ordinal].find(first_ordinal);
    EXPECT_EQ(ext_1, it->second);
    ++it;
    EXPECT_EQ(ext_2, it->second);

    app_sorting()->RemoveOrdinalMapping(ext_1, first_ordinal, first_ordinal);
    EXPECT_EQ(1U, app_sorting()->ntp_ordinal_map_.size());
    EXPECT_EQ(1U, app_sorting()->ntp_ordinal_map_[first_ordinal].size());

    it = app_sorting()->ntp_ordinal_map_[first_ordinal].find(first_ordinal);
    EXPECT_EQ(ext_2, it->second);

    // Ensure that attempting to remove an extension with a valid page and app
    // launch ordinals, but a unused id has no effect.
    app_sorting()->RemoveOrdinalMapping(
        "invalid_ext", first_ordinal, first_ordinal);
    EXPECT_EQ(1U, app_sorting()->ntp_ordinal_map_.size());
    EXPECT_EQ(1U, app_sorting()->ntp_ordinal_map_[first_ordinal].size());

    it = app_sorting()->ntp_ordinal_map_[first_ordinal].find(first_ordinal);
    EXPECT_EQ(ext_2, it->second);
  }
};
TEST_F(ChromeAppSortingPageOrdinalMapping,
       ChromeAppSortingPageOrdinalMapping) {}

class ChromeAppSortingPreinstalledAppsBase : public PrefsPrepopulatedTestBase {
 public:
  ChromeAppSortingPreinstalledAppsBase() {
    base::DictionaryValue simple_dict;
    simple_dict.SetString(keys::kVersion, "1.0.0.0");
    simple_dict.SetString(keys::kName, "unused");
    simple_dict.SetString(keys::kApp, "true");
    simple_dict.SetString(keys::kLaunchLocalPath, "fake.html");

    std::string error;
    app1_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("app1_"), Manifest::EXTERNAL_PREF,
        simple_dict, Extension::NO_FLAGS, &error);
    prefs()->OnExtensionInstalled(app1_scoped_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());

    app2_scoped_ = Extension::Create(
        prefs_.temp_dir().AppendASCII("app2_"), Manifest::EXTERNAL_PREF,
        simple_dict, Extension::NO_FLAGS, &error);
    prefs()->OnExtensionInstalled(app2_scoped_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());

    app1_ = app1_scoped_.get();
    app2_ = app2_scoped_.get();
  }
  ~ChromeAppSortingPreinstalledAppsBase() override {}

 protected:
  // Weak references, for convenience.
  Extension* app1_;
  Extension* app2_;

 private:
  scoped_refptr<Extension> app1_scoped_;
  scoped_refptr<Extension> app2_scoped_;
};

class ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage
    : public ChromeAppSortingPreinstalledAppsBase {
 public:
  ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage() {}
  ~ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage() override {}

  void Initialize() override {}
  void Verify() override {
    syncer::StringOrdinal page = syncer::StringOrdinal::CreateInitialOrdinal();

    syncer::StringOrdinal min =
        app_sorting()->GetMinOrMaxAppLaunchOrdinalsOnPage(
            page,
            ChromeAppSorting::MIN_ORDINAL);
    syncer::StringOrdinal max =
        app_sorting()->GetMinOrMaxAppLaunchOrdinalsOnPage(
            page,
            ChromeAppSorting::MAX_ORDINAL);
    EXPECT_TRUE(min.IsValid());
    EXPECT_TRUE(max.IsValid());
    EXPECT_TRUE(min.LessThan(max));

    // Ensure that the min and max values aren't set for empty pages.
    min = syncer::StringOrdinal();
    max = syncer::StringOrdinal();
    syncer::StringOrdinal empty_page = page.CreateAfter();
    EXPECT_FALSE(min.IsValid());
    EXPECT_FALSE(max.IsValid());
    min = app_sorting()->GetMinOrMaxAppLaunchOrdinalsOnPage(
        empty_page,
        ChromeAppSorting::MIN_ORDINAL);
    max = app_sorting()->GetMinOrMaxAppLaunchOrdinalsOnPage(
        empty_page,
        ChromeAppSorting::MAX_ORDINAL);
    EXPECT_FALSE(min.IsValid());
    EXPECT_FALSE(max.IsValid());
  }
};
TEST_F(ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage,
       ChromeAppSortingGetMinOrMaxAppLaunchOrdinalsOnPage) {}

// Make sure that empty pages aren't removed from the integer to ordinal
// mapping. See http://crbug.com/109802 for details.
class ChromeAppSortingKeepEmptyStringOrdinalPages
    : public ChromeAppSortingPreinstalledAppsBase {
 public:
  ChromeAppSortingKeepEmptyStringOrdinalPages() {}
  ~ChromeAppSortingKeepEmptyStringOrdinalPages() override {}

  void Initialize() override {
    syncer::StringOrdinal first_page =
        syncer::StringOrdinal::CreateInitialOrdinal();
    app_sorting()->SetPageOrdinal(app1_->id(), first_page);
    EXPECT_EQ(0, app_sorting()->PageStringOrdinalAsInteger(first_page));

    last_page_ = first_page.CreateAfter();
    app_sorting()->SetPageOrdinal(app2_->id(), last_page_);
    EXPECT_EQ(1, app_sorting()->PageStringOrdinalAsInteger(last_page_));

    // Move the second app to create an empty page.
    app_sorting()->SetPageOrdinal(app2_->id(), first_page);
    EXPECT_EQ(0, app_sorting()->PageStringOrdinalAsInteger(first_page));
  }
  void Verify() override {
    // Move the second app to a new empty page at the end, skipping over
    // the current empty page.
    last_page_ = last_page_.CreateAfter();
    app_sorting()->SetPageOrdinal(app2_->id(), last_page_);
    EXPECT_EQ(2, app_sorting()->PageStringOrdinalAsInteger(last_page_));
    EXPECT_TRUE(
        last_page_.Equals(app_sorting()->PageIntegerAsStringOrdinal(2)));
  }

 private:
  syncer::StringOrdinal last_page_;
};
TEST_F(ChromeAppSortingKeepEmptyStringOrdinalPages,
       ChromeAppSortingKeepEmptyStringOrdinalPages) {}

class ChromeAppSortingMakesFillerOrdinals
    : public ChromeAppSortingPreinstalledAppsBase {
 public:
  ChromeAppSortingMakesFillerOrdinals() {}
  ~ChromeAppSortingMakesFillerOrdinals() override {}

  void Initialize() override {
    syncer::StringOrdinal first_page =
        syncer::StringOrdinal::CreateInitialOrdinal();
    app_sorting()->SetPageOrdinal(app1_->id(), first_page);
    EXPECT_EQ(0, app_sorting()->PageStringOrdinalAsInteger(first_page));
  }
  void Verify() override {
    // Because the UI can add an unlimited number of empty pages without an app
    // on them, this test simulates dropping of an app on the 1st and 4th empty
    // pages (3rd and 6th pages by index) to ensure we don't crash and that
    // filler ordinals are created as needed. See: http://crbug.com/122214
    syncer::StringOrdinal page_three =
        app_sorting()->PageIntegerAsStringOrdinal(2);
    app_sorting()->SetPageOrdinal(app1_->id(), page_three);
    EXPECT_EQ(2, app_sorting()->PageStringOrdinalAsInteger(page_three));

    syncer::StringOrdinal page_six =
        app_sorting()->PageIntegerAsStringOrdinal(5);
    app_sorting()->SetPageOrdinal(app1_->id(), page_six);
    EXPECT_EQ(5, app_sorting()->PageStringOrdinalAsInteger(page_six));
  }
};
TEST_F(ChromeAppSortingMakesFillerOrdinals,
       ChromeAppSortingMakesFillerOrdinals) {}

class ChromeAppSortingDefaultOrdinalsBase : public ExtensionPrefsTest {
 public:
  ChromeAppSortingDefaultOrdinalsBase() {}
  ~ChromeAppSortingDefaultOrdinalsBase() override {}

  void Initialize() override {
    app_ = CreateApp("app");

    InitDefaultOrdinals();
    ChromeAppSorting::AppOrdinalsMap& sorting_defaults =
        app_sorting()->default_ordinals_;
    sorting_defaults[app_->id()].page_ordinal = default_page_ordinal_;
    sorting_defaults[app_->id()].app_launch_ordinal =
        default_app_launch_ordinal_;

    SetupUserOrdinals();
    InstallApps();
  }

 protected:
  scoped_refptr<Extension> CreateApp(const std::string& name) {
    base::DictionaryValue simple_dict;
    simple_dict.SetString(keys::kVersion, "1.0.0.0");
    simple_dict.SetString(keys::kName, name);
    simple_dict.SetString(keys::kApp, "true");
    simple_dict.SetString(keys::kLaunchLocalPath, "fake.html");

    std::string errors;
    scoped_refptr<Extension> app = Extension::Create(
        prefs_.temp_dir().AppendASCII(name), Manifest::EXTERNAL_PREF,
        simple_dict, Extension::NO_FLAGS, &errors);
    EXPECT_TRUE(app.get()) << errors;
    EXPECT_TRUE(crx_file::id_util::IdIsValid(app->id()));
    return app;
  }

  void InitDefaultOrdinals() {
    default_page_ordinal_ =
        syncer::StringOrdinal::CreateInitialOrdinal().CreateAfter();
    default_app_launch_ordinal_ =
        syncer::StringOrdinal::CreateInitialOrdinal().CreateBefore();
  }

  virtual void SetupUserOrdinals() {}

  virtual void InstallApps() {
    prefs()->OnExtensionInstalled(app_.get(),
                                  Extension::ENABLED,
                                  syncer::StringOrdinal(),
                                  std::string());
  }

  scoped_refptr<Extension> app_;
  syncer::StringOrdinal default_page_ordinal_;
  syncer::StringOrdinal default_app_launch_ordinal_;
};

// Tests that the app gets its default ordinals.
class ChromeAppSortingDefaultOrdinals
    : public ChromeAppSortingDefaultOrdinalsBase {
 public:
  ChromeAppSortingDefaultOrdinals() {}
  ~ChromeAppSortingDefaultOrdinals() override {}

  void Verify() override {
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(app_->id()).Equals(
        default_page_ordinal_));
    EXPECT_TRUE(app_sorting()->GetAppLaunchOrdinal(app_->id()).Equals(
        default_app_launch_ordinal_));
  }
};
TEST_F(ChromeAppSortingDefaultOrdinals,
       ChromeAppSortingDefaultOrdinals) {}

// Tests that the default page ordinal is overridden by install page ordinal.
class ChromeAppSortingDefaultOrdinalOverriddenByInstallPage
    : public ChromeAppSortingDefaultOrdinalsBase {
 public:
  ChromeAppSortingDefaultOrdinalOverriddenByInstallPage() {}
  ~ChromeAppSortingDefaultOrdinalOverriddenByInstallPage() override {}

  void Verify() override {
    EXPECT_FALSE(app_sorting()->GetPageOrdinal(app_->id()).Equals(
        default_page_ordinal_));
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(app_->id()).Equals(
        install_page_));
  }

 protected:
  void InstallApps() override {
    install_page_ = default_page_ordinal_.CreateAfter();
    prefs()->OnExtensionInstalled(app_.get(),
                                  Extension::ENABLED,
                                  install_page_,
                                  std::string());
  }

 private:
  syncer::StringOrdinal install_page_;
};
TEST_F(ChromeAppSortingDefaultOrdinalOverriddenByInstallPage,
       ChromeAppSortingDefaultOrdinalOverriddenByInstallPage) {}

// Tests that the default ordinals are overridden by user values.
class ChromeAppSortingDefaultOrdinalOverriddenByUserValue
    : public ChromeAppSortingDefaultOrdinalsBase {
 public:
  ChromeAppSortingDefaultOrdinalOverriddenByUserValue() {}
  ~ChromeAppSortingDefaultOrdinalOverriddenByUserValue() override {}

  void Verify() override {
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(app_->id()).Equals(
        user_page_ordinal_));
    EXPECT_TRUE(app_sorting()->GetAppLaunchOrdinal(app_->id()).Equals(
        user_app_launch_ordinal_));
  }

 protected:
  void SetupUserOrdinals() override {
    user_page_ordinal_ = default_page_ordinal_.CreateAfter();
    user_app_launch_ordinal_ = default_app_launch_ordinal_.CreateBefore();

    app_sorting()->SetPageOrdinal(app_->id(), user_page_ordinal_);
    app_sorting()->SetAppLaunchOrdinal(app_->id(), user_app_launch_ordinal_);
  }

 private:
  syncer::StringOrdinal user_page_ordinal_;
  syncer::StringOrdinal user_app_launch_ordinal_;
};
TEST_F(ChromeAppSortingDefaultOrdinalOverriddenByUserValue,
       ChromeAppSortingDefaultOrdinalOverriddenByUserValue) {}

// Tests that the default app launch ordinal is changed to avoid collision.
class ChromeAppSortingDefaultOrdinalNoCollision
    : public ChromeAppSortingDefaultOrdinalsBase {
 public:
  ChromeAppSortingDefaultOrdinalNoCollision() {}
  ~ChromeAppSortingDefaultOrdinalNoCollision() override {}

  void Verify() override {
    // Use the default page.
    EXPECT_TRUE(app_sorting()->GetPageOrdinal(app_->id()).Equals(
        default_page_ordinal_));
    // Not using the default app launch ordinal because of the collision.
    EXPECT_FALSE(app_sorting()->GetAppLaunchOrdinal(app_->id()).Equals(
        default_app_launch_ordinal_));
  }

 protected:
  void SetupUserOrdinals() override {
    other_app_ = prefs_.AddApp("other_app");
    // Creates a collision.
    app_sorting()->SetPageOrdinal(other_app_->id(), default_page_ordinal_);
    app_sorting()->SetAppLaunchOrdinal(other_app_->id(),
                                       default_app_launch_ordinal_);

    yet_another_app_ = prefs_.AddApp("yet_aother_app");
    app_sorting()->SetPageOrdinal(yet_another_app_->id(),
                                  default_page_ordinal_);
    app_sorting()->SetAppLaunchOrdinal(yet_another_app_->id(),
                                       default_app_launch_ordinal_);
  }

 private:
  scoped_refptr<Extension> other_app_;
  scoped_refptr<Extension> yet_another_app_;
};
TEST_F(ChromeAppSortingDefaultOrdinalNoCollision,
       ChromeAppSortingDefaultOrdinalNoCollision) {}

// Tests that SetExtensionVisible() correctly hides and unhides extensions.
class ChromeAppSortingSetExtensionVisible : public ExtensionPrefsTest {
 public:
  ChromeAppSortingSetExtensionVisible() {}
  ~ChromeAppSortingSetExtensionVisible() override {}

  void Initialize() override {
    first_app_ = prefs_.AddApp("first_app");
    second_app_ = prefs_.AddApp("second_app");
  }

  void Verify() override {
    ChromeAppSorting* sorting = app_sorting();
    syncer::StringOrdinal page1 = sorting->GetPageOrdinal(first_app_->id());
    syncer::StringOrdinal page2 = sorting->GetPageOrdinal(second_app_->id());
    EXPECT_TRUE(sorting->GetAppLaunchOrdinal(first_app_->id()).IsValid());
    EXPECT_TRUE(sorting->GetAppLaunchOrdinal(second_app_->id()).IsValid());
    EXPECT_TRUE(page1.IsValid());
    EXPECT_TRUE(page2.IsValid());
    EXPECT_TRUE(page1.Equals(page2));

    sorting->SetExtensionVisible(first_app_->id(), false);
    EXPECT_EQ(
        1U, sorting->CountItemsVisibleOnNtp(sorting->ntp_ordinal_map_[page1]));

    sorting->SetExtensionVisible(first_app_->id(), true);
    EXPECT_EQ(
        2U, sorting->CountItemsVisibleOnNtp(sorting->ntp_ordinal_map_[page1]));
  }

 private:
  scoped_refptr<Extension> first_app_;
  scoped_refptr<Extension> second_app_;
};
TEST_F(ChromeAppSortingSetExtensionVisible,
       ChromeAppSortingSetExtensionVisible) {
}

}  // namespace extensions
