// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#include "build/build_config.h"
#include "ui/gfx/switches.h"

// New headless mode is available on Linux, Windows and Mac platforms.
// More platforms will be added later, so avoid function level clutter
// by providing stub implementations at the end of the file.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#include "base/base_switches.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/headless/headless_mode_switches.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_LINUX)
#include "ui/gl/gl_switches.h"               // nogncheck
#include "ui/ozone/public/ozone_switches.h"  // nogncheck
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
#include "chrome/chrome_elf/chrome_elf_main.h"
#endif  // BUILDFLAG(IS_WIN)

namespace headless {

namespace {
const char kNewHeadlessModeSwitchValue[] = "new";
const char kOldHeadlessModeSwitchValue[] = "old";

enum HeadlessMode {
  kNoHeadlessMode,
  kOldHeadlessMode,
  kNewHeadlessMode,
  kDefaultHeadlessMode = kNewHeadlessMode
};

HeadlessMode GetHeadlessMode() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kHeadless))
    return kNoHeadlessMode;

  std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kHeadless);
  if (switch_value == kOldHeadlessModeSwitchValue)
    return kOldHeadlessMode;
  if (switch_value == kNewHeadlessModeSwitchValue)
    return kNewHeadlessMode;

  return kDefaultHeadlessMode;
}

class HeadlessModeHandleImpl : public HeadlessModeHandle {
 public:
  HeadlessModeHandleImpl() { SetUpCommandLine(); }

  ~HeadlessModeHandleImpl() override = default;

 private:
  void SetUpCommandLine() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    // Default to incognito mode unless it is forced or user data directory is
    // explicitly specified.
    if (!command_line->HasSwitch(::switches::kIncognito) &&
        !command_line->HasSwitch(::switches::kUserDataDir)) {
      command_line->AppendSwitch(::switches::kIncognito);
    }

    // Enable unattended mode.
    if (!command_line->HasSwitch(::switches::kNoErrorDialogs)) {
      command_line->AppendSwitch(::switches::kNoErrorDialogs);
    }

    // Disable first run user experience.
    if (!command_line->HasSwitch(::switches::kNoFirstRun)) {
      command_line->AppendSwitch(::switches::kNoFirstRun);
    }

    // Excplicitely specify unique user data dir because if there is no one
    // provided, Chrome will fall back to the default one which will prevent
    // parallel headless processes execution, see https://crbug.com/1477376.
    if (!command_line->HasSwitch(::switches::kUserDataDir) &&
        !command_line->HasSwitch(::switches::kProcessType)) {
      const base::FilePath& user_data_dir = GetUserDataDir();
      if (!user_data_dir.empty()) {
        command_line->AppendSwitchPath(switches::kUserDataDir, user_data_dir);
      }
    }

#if BUILDFLAG(IS_LINUX)
  // Headless mode on Linux relies on ozone/headless platform.
  command_line->AppendSwitchASCII(::switches::kOzonePlatform,
                                  switches::kHeadless);
  if (!command_line->HasSwitch(switches::kOzoneOverrideScreenSize)) {
    command_line->AppendSwitchASCII(switches::kOzoneOverrideScreenSize,
                                    "800,600");
  }

  // If Ozone/Headless is enabled, Vulkan initialization crashes unless
  // Angle implementation is specified explicitly.
  if (!command_line->HasSwitch(switches::kUseGL) &&
      !command_line->HasSwitch(switches::kUseANGLE) &&
      !command_line->HasSwitch(switches::kEnableGPU)) {
    command_line->AppendSwitchASCII(
        switches::kUseANGLE, gl::kANGLEImplementationSwiftShaderForWebGLName);
  }
#endif  // BUILDFLAG(IS_LINUX)
  }

  const base::FilePath& GetUserDataDir() {
    if (!user_data_dir_.IsValid()) {
#if BUILDFLAG(IS_WIN)
      // On Windows user data dir is handled before chrome.dll is loaded in
      // chrome_elf, see chrome/install_static/user_data_dir.h/cc, so check to
      // see if the temporary data dir for headless mode was created there and
      // if so, associate it with our code for cleanup on exit.
      if (IsTemporaryUserDataDirectoryCreatedForHeadless()) {
        wchar_t user_data_dir_buf[MAX_PATH],
            invalid_user_data_dir_buf[MAX_PATH];
        if (GetUserDataDirectoryThunk(user_data_dir_buf,
                                      std::size(user_data_dir_buf),
                                      invalid_user_data_dir_buf,
                                      std::size(invalid_user_data_dir_buf))) {
          base::FilePath user_data_dir(user_data_dir_buf);
          if (!user_data_dir.empty()) {
            CHECK(user_data_dir_.Set(user_data_dir));
          }
        }
      }
#else   // BUILDFLAG(IS_WIN)
      CHECK(user_data_dir_.CreateUniqueTempDir());
#endif  // BUILDFLAG(IS_WIN)
    }

    return user_data_dir_.GetPath();
  }

  base::ScopedTempDir user_data_dir_;
};

}  // namespace

bool IsHeadlessMode() {
  return GetHeadlessMode() == kNewHeadlessMode;
}

bool IsOldHeadlessMode() {
  return GetHeadlessMode() == kOldHeadlessMode;
}

bool IsChromeSchemeUrlAllowed() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kAllowChromeSchemeUrl);
}

std::unique_ptr<HeadlessModeHandle> InitHeadlessMode() {
  CHECK(IsHeadlessMode());

  return std::make_unique<HeadlessModeHandleImpl>();
}

}  // namespace headless

#else  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace headless {

bool IsHeadlessMode() {
  return false;
}

bool IsOldHeadlessMode() {
  // In addition to Linux, Windows and Mac (which are handled above),
  // the old headless mode is also supported on ChromeOS, see chrome_main.cc.
#if BUILDFLAG(IS_CHROMEOS)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kHeadless);
#else
  return false;
#endif
}

bool IsChromeSchemeUrlAllowed() {
  return false;
}

void SetUpCommandLine(const base::CommandLine* command_line) {}

std::unique_ptr<HeadlessModeHandle> InitHeadlessMode() {
  return nullptr;
}

}  // namespace headless

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
