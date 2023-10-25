// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_FOR_WHICH_EXTENSION_TYPE_H_
#define CHROME_BROWSER_LACROS_FOR_WHICH_EXTENSION_TYPE_H_

namespace extensions {

class Extension;

}  // namespace extensions

// A class to embody instance-specific specialization for one of {Chrome Apps,
// Extension}.
class ForWhichExtensionType {
 public:
  ForWhichExtensionType(const ForWhichExtensionType&);

  virtual ~ForWhichExtensionType();

  ForWhichExtensionType& operator=(const ForWhichExtensionType&) = delete;

  // Returns whether |extension| has a matching type.
  bool Matches(const extensions::Extension* extension) const;

  // Predicates to decide spedialization. These cases are mutual exclusive, but
  // to maintain semantics, callers should not rely on this fact.
  bool IsChromeApps() const { return for_chrome_apps_; }
  bool IsExtensions() const { return !for_chrome_apps_; }

  // Helper to choose a value, depending whether specialization is for
  // Chrome Apps or Extensions.
  template <class T>
  const T& ChooseForChromeAppOrExtension(const T& val_for_chrome_apps,
                                         const T& val_for_extensions) const {
    return for_chrome_apps_ ? val_for_chrome_apps : val_for_extensions;
  }

  template <class T>
  const T& ChooseIntentFilter(bool is_legacy_quick_office,
                              const T& val_for_chrome_apps,
                              const T& val_for_extensions) const {
    if (is_legacy_quick_office || for_chrome_apps_) {
      return val_for_chrome_apps;
    }

    return val_for_extensions;
  }

 private:
  friend ForWhichExtensionType InitForChromeApps();
  friend ForWhichExtensionType InitForExtensions();

  explicit ForWhichExtensionType(bool for_chrome_apps);

  // Opaque specialization state: Chrome App (if true) or Extension (if false).
  const bool for_chrome_apps_;
};

// Returns ForWhichExtensionType value for Chrome Apps specialization.
ForWhichExtensionType InitForChromeApps();

// Returns ForWhichExtensionType value for Extensions specialization.
ForWhichExtensionType InitForExtensions();

#endif  // CHROME_BROWSER_LACROS_FOR_WHICH_EXTENSION_TYPE_H_
