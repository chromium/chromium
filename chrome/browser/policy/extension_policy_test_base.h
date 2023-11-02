// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_EXTENSION_POLICY_TEST_BASE_H_
#define CHROME_BROWSER_POLICY_EXTENSION_POLICY_TEST_BASE_H_

#include "base/files/file_path.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace policy {

extern const base::FilePath::CharType kTestExtensionsDir[];

class ExtensionPolicyTestBase : public PolicyTest {
 protected:
  ExtensionPolicyTestBase();
  ~ExtensionPolicyTestBase() override;

  scoped_refptr<const extensions::Extension> LoadUnpackedExtension(
      const base::FilePath::StringType& name);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_EXTENSION_POLICY_TEST_BASE_H_
