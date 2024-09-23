// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include <memory>

#include "base/command_line.h"
#include "base/environment.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_switches.h"

namespace {

// A class that over-writes the system locale only in a scope. To emulate the
// specified environment on Linux, this class over-writes a LC_ALL environment
// variable when creating a LocaleTest object and restore it with the original
// value when deleting the object. (This environment variable may affect other
// tests and we have to restore it regardless of the results of LocaleTests.)
class ScopedLocale {
 public:
  explicit ScopedLocale(const char* locale) : locale_(locale) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    old_locale_ = getenv("LC_ALL");

    static const struct {
      const char* chrome_locale;
      const char* system_locale;
    } kLocales[] = {
      { "da", "da_DK.UTF-8" },
      { "he", "he_IL.UTF-8" },
      { "zh-TW", "zh_TW.UTF-8" }
    };
    bool found_locale = false;
    for (size_t i = 0; i < std::size(kLocales); ++i) {
      if (kLocales[i].chrome_locale == locale) {
        found_locale = true;
        setenv("LC_ALL", kLocales[i].system_locale, 1);
      }
    }
    DCHECK(found_locale);
#endif
  }

  ~ScopedLocale() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    if (old_locale_) {
      env->SetVar("LC_ALL", old_locale_);
    } else {
      env->UnSetVar("LC_ALL");
    }
#endif
  }

  const std::string& locale() { return locale_; }

 private:
  std::string locale_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const char* old_locale_;
#endif
};

// A base class for tests used in this file. This class over-writes the system
// locale and run Chrome with a '--lang' option. To add a new LocaleTest, add a
// class derived from this class and call the constructor with the locale name
// used by Chrome.
class LocaleTestBase : public InProcessBrowserTest {
 public:
  explicit LocaleTestBase(const char* locale) : locale_(locale) {
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kLang, locale_.locale());
  }

 protected:
  ScopedLocale locale_;
};

// Test classes that run Chrome on the Danish locale, the Hebrew locale, and
// the Traditional-Chinese locale, respectively.
class LocaleTestDanish : public LocaleTestBase {
 public:
  LocaleTestDanish() : LocaleTestBase("da") {
  }
};

class LocaleTestHebrew : public LocaleTestBase {
 public:
  LocaleTestHebrew() : LocaleTestBase("he") {
  }
};

class LocaleTestTraditionalChinese : public LocaleTestBase {
 public:
  LocaleTestTraditionalChinese() : LocaleTestBase("zh-TW") {
  }
};

}  // namespace

// Start Chrome and shut it down on the Danish locale, the Hebrew locale, and
// the Traditional-Chinese locale, respectively. These tests do not need any
// code here because they just verify we can start Chrome and shut it down
// cleanly on these locales.
IN_PROC_BROWSER_TEST_F(LocaleTestDanish, TestStart) {
}

IN_PROC_BROWSER_TEST_F(LocaleTestHebrew, TestStart) {
}

IN_PROC_BROWSER_TEST_F(LocaleTestTraditionalChinese, TestStart) {
}
