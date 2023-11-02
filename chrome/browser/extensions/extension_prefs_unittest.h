// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_

#include <stddef.h>

#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace extensions {
class ChromeAppSorting;
class Extension;

// Base class for extension preference-related unit tests.
class ExtensionPrefsTest : public testing::Test {
 public:
  ExtensionPrefsTest();

  ExtensionPrefsTest(const ExtensionPrefsTest&) = delete;
  ExtensionPrefsTest& operator=(const ExtensionPrefsTest&) = delete;

  ~ExtensionPrefsTest() override;

  // This function will get called once, and is the right place to do operations
  // on ExtensionPrefs that write data.
  virtual void Initialize() = 0;

  // This function will be called twice - once while the original ExtensionPrefs
  // object is still alive, and once after recreation. Thus, it tests that
  // things don't break after any ExtensionPrefs startup work.
  virtual void Verify() = 0;

  // This function is called to Register preference default values.
  virtual void RegisterPreferences(user_prefs::PrefRegistrySyncable* registry);

  void SetUp() override;

  void TearDown() override;

 protected:
  ExtensionPrefs* prefs() { return prefs_.prefs(); }
  ChromeAppSorting* app_sorting() { return prefs_.app_sorting(); }

  content::BrowserTaskEnvironment task_environment_;
  TestExtensionPrefs prefs_;
};


class PrefsPrepopulatedTestBase : public ExtensionPrefsTest {
 public:
  static const size_t kNumInstalledExtensions = 5;

  PrefsPrepopulatedTestBase();

  PrefsPrepopulatedTestBase(const PrefsPrepopulatedTestBase&) = delete;
  PrefsPrepopulatedTestBase& operator=(const PrefsPrepopulatedTestBase&) =
      delete;

  ~PrefsPrepopulatedTestBase() override;

  Extension* extension1() { return extension1_.get(); }
  Extension* extension2() { return extension2_.get(); }
  Extension* extension3() { return extension3_.get(); }
  Extension* extension4() { return extension4_.get(); }
  Extension* internal_extension() { return internal_extension_.get(); }

 protected:
  bool installed_[kNumInstalledExtensions];

  // The following extensions all have mojom::ManifestLocation set to
  // mojom::ManifestLocation::kExternalPref.
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;

  // This extension has a location of mojom::ManifestLocation::kInternal.
  scoped_refptr<Extension> internal_extension_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_
