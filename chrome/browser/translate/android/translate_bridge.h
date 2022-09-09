// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_ANDROID_TRANSLATE_BRIDGE_H_
#define CHROME_BROWSER_TRANSLATE_ANDROID_TRANSLATE_BRIDGE_H_

#include <string>

class TranslateBridge {
 public:
  // Use |locale| to create a language-region pair and language code to prepend
  // to the default accept languages. For Malay, we'll end up creating
  // "ms-MY,ms,en-US,en", and for Swiss-German, we will have
  // "de-CH,de-DE,de,en-US,en".
  static void PrependToAcceptLanguagesIfNecessary(
      const std::string& locale,
      std::string* accept_languages);
};

#endif  // CHROME_BROWSER_TRANSLATE_ANDROID_TRANSLATE_BRIDGE_H_
