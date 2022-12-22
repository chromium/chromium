// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/fake_cups_print_job_manager.h"

namespace ash {

// FakeCupsPrintJobManager is available whether CUPS is available or not (so it
// can be used standalone in unit tests).  However, we only want to replace the
// default implementation when CUPS is not available, so this is broken out from
// the fake_cups_print_job_manager.cc file.

// static
CupsPrintJobManager* CupsPrintJobManager::CreateInstance(Profile* profile) {
  return new FakeCupsPrintJobManager(profile);
}

}  // namespace ash
