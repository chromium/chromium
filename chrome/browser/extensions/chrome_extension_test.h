// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;
class TestingProfile;

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionRegistrar;
class ExtensionRegistry;

// A canonical unit test class for exercising components of the extensions
// platform. This test suite will create a test profile and, along with it, the
// various extension services that are necessary. It also includes some handy
// accessors for extremely common utilities.
//
// This is not meant to be a catch-all for *everything* you may need to do in
// a unit test. Please add niche / bespoke functionality to derived test
// suites (or helper classes, or elsewhere).
//
// Note: If you are just testing functionality from the //extensions layer,
// please prefer placing your unit test there and adding them to the
// `extensions_unittests` build target. Unlike browser tests (which can be more
// "end-to-end" or integration tests), unit tests *should* be targeted, so
// please place them proximal to the code under test.
class ChromeExtensionTest : public testing::Test {
 public:
  ChromeExtensionTest();
  ChromeExtensionTest(const ChromeExtensionTest&) = delete;
  ChromeExtensionTest& operator=(const ChromeExtensionTest&) = delete;
  ~ChromeExtensionTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }
  ExtensionRegistrar* extension_registrar() {
    return extension_registrar_.get();
  }
  ExtensionRegistry* extension_registry() { return extension_registry_.get(); }
  // Note: Defined in the .cc file so that we don't need to include
  // testing_profile.h here.
  Profile* profile();
  // Note: Defined in the .cc file so that we don't need to include profile.h
  // here.
  content::BrowserContext* browser_context();
  TestingProfile* testing_profile() { return profile_.get(); }

 private:
  // Must be constructed before any other members that conceivably care about
  // tasks. So, put it first.
  content::BrowserTaskEnvironment task_environment_;

  // Owns KeyedServices (though these are torn down before test destruction).
  std::unique_ptr<TestingProfile> profile_;

  raw_ptr<ExtensionRegistrar> extension_registrar_;
  raw_ptr<ExtensionRegistry> extension_registry_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_TEST_H_
