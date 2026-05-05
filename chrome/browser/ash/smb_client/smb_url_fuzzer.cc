// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smb_url.h"

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"

// This is a workaround for https://crbug.com/40546867.
struct IcuEnvironment {
  IcuEnvironment() {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    CHECK(base::i18n::InitializeICU());
  }
  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static base::NoDestructor<IcuEnvironment> env;
  std::string input(reinterpret_cast<const char*>(data), size);

  ash::smb_client::SmbUrl url(input);
  return 0;
}
