// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/diagnostics/diagnostics_model.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/diagnostics/diagnostics_test.h"
#include "chrome/browser/diagnostics/recon_diagnostics.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"

namespace diagnostics {

// This is the count of diagnostic tests on each platform.  This should
// only be used by testing code.
#if BUILDFLAG(IS_WIN)
const int DiagnosticsModel::kDiagnosticsTestCount = 18;
#elif BUILDFLAG(IS_MAC)
const int DiagnosticsModel::kDiagnosticsTestCount = 15;
#elif BUILDFLAG(IS_POSIX)
#if BUILDFLAG(IS_CHROMEOS_ASH)
const int DiagnosticsModel::kDiagnosticsTestCount = 19;
#else
const int DiagnosticsModel::kDiagnosticsTestCount = 17;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_WIN)

namespace {

// Embodies the commonalities of the model across platforms. It manages the
// list of tests and can loop over them. The main job of the platform specific
// code becomes:
// 1- Inserting the appropriate tests into |tests_|
// 2- Overriding RunTest() to wrap it with the appropriate fatal exception
//    handler for the OS.
// This class owns the all the tests and will only delete them upon
// destruction.
class DiagnosticsModelImpl : public DiagnosticsModel {
 public:
  DiagnosticsModelImpl() : tests_run_(0) {}

  DiagnosticsModelImpl(const DiagnosticsModelImpl&) = delete;
  DiagnosticsModelImpl& operator=(const DiagnosticsModelImpl&) = delete;

  ~DiagnosticsModelImpl() override {}

  int GetTestRunCount() const override { return tests_run_; }

  int GetTestAvailableCount() const override { return tests_.size(); }

  void RunAll(DiagnosticsModel::Observer* observer) override {
    size_t test_count = tests_.size();
    bool continue_running = true;
    for (size_t i = 0; i != test_count; ++i) {
      if (continue_running) {
        continue_running = RunTest(tests_[i].get(), observer, i);
        ++tests_run_;
      } else {
        // Just bail out if a recovery step returns false.
        break;
      }
    }
    if (observer)
      observer->OnAllTestsDone(this);
  }

  void RecoverAll(DiagnosticsModel::Observer* observer) override {
    size_t test_count = tests_.size();
    bool continue_running = true;
    for (size_t i = 0; i != test_count; ++i) {
      if (continue_running) {
        continue_running = RunRecovery(tests_[i].get(), observer, i);
      } else {
        // Just bail out if a recovery step returns false.
        break;
      }
    }
    if (observer)
      observer->OnAllRecoveryDone(this);
  }

  const TestInfo& GetTest(size_t index) const override {
    return *tests_[index];
  }

  bool GetTestInfo(int id, const TestInfo** result) const override {
    DCHECK(id >= 0);
    for (const auto& test : tests_) {
      if (test->GetId() == id) {
        *result = test.get();
        return true;
      }
    }
    return false;
  }

 protected:
  // Run a particular diagnostic test. Return false if no other tests should be
  // run.
  virtual bool RunTest(DiagnosticsTest* test,
                       Observer* observer,
                       size_t index) {
    return test->Execute(observer, this, index);
  }

  // Recover from a particular diagnostic test. Return false if no further
  // recovery should be run.
  virtual bool RunRecovery(DiagnosticsTest* test,
                           Observer* observer,
                           size_t index) {
    return test->Recover(observer, this, index);
  }

  std::vector<std::unique_ptr<DiagnosticsTest>> tests_;
  int tests_run_;
};

// Each platform can have their own tests. For the time being there is only
// one test that works on all platforms.
#if BUILDFLAG(IS_WIN)
class DiagnosticsModelWin : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelWin() {
    tests_.push_back(MakeOperatingSystemTest());
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeVersionTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeResourcesFileTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeLocalOrSyncableBookmarksTest());
    tests_.push_back(MakeAccountBookmarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteFaviconsDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteTopSitesDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
  }

  DiagnosticsModelWin(const DiagnosticsModelWin&) = delete;
  DiagnosticsModelWin& operator=(const DiagnosticsModelWin&) = delete;
};

#elif BUILDFLAG(IS_MAC)
class DiagnosticsModelMac : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelMac() {
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeLocalOrSyncableBookmarksTest());
    tests_.push_back(MakeAccountBookmarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteFaviconsDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteTopSitesDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
  }

  DiagnosticsModelMac(const DiagnosticsModelMac&) = delete;
  DiagnosticsModelMac& operator=(const DiagnosticsModelMac&) = delete;
};

#elif BUILDFLAG(IS_POSIX)
class DiagnosticsModelPosix : public DiagnosticsModelImpl {
 public:
  DiagnosticsModelPosix() {
    tests_.push_back(MakeInstallTypeTest());
    tests_.push_back(MakeVersionTest());
    tests_.push_back(MakeUserDirTest());
    tests_.push_back(MakeLocalStateFileTest());
    tests_.push_back(MakeDictonaryDirTest());
    tests_.push_back(MakeResourcesFileTest());
    tests_.push_back(MakeDiskSpaceTest());
    tests_.push_back(MakePreferencesTest());
    tests_.push_back(MakeLocalStateTest());
    tests_.push_back(MakeLocalOrSyncableBookmarksTest());
    tests_.push_back(MakeAccountBookmarksTest());
    tests_.push_back(MakeSqliteWebDataDbTest());
    tests_.push_back(MakeSqliteCookiesDbTest());
    tests_.push_back(MakeSqliteFaviconsDbTest());
    tests_.push_back(MakeSqliteHistoryDbTest());
    tests_.push_back(MakeSqliteTopSitesDbTest());
    tests_.push_back(MakeSqliteWebDatabaseTrackerDbTest());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    tests_.push_back(MakeSqliteNssCertDbTest());
    tests_.push_back(MakeSqliteNssKeyDbTest());
#endif
  }

  DiagnosticsModelPosix(const DiagnosticsModelPosix&) = delete;
  DiagnosticsModelPosix& operator=(const DiagnosticsModelPosix&) = delete;
};

#endif

}  // namespace

DiagnosticsModel* MakeDiagnosticsModel(const base::CommandLine& cmdline) {
  base::FilePath user_data_dir =
      cmdline.GetSwitchValuePath(switches::kUserDataDir);
  if (!user_data_dir.empty())
    base::PathService::Override(chrome::DIR_USER_DATA, user_data_dir);
#if BUILDFLAG(IS_WIN)
  return new DiagnosticsModelWin();
#elif BUILDFLAG(IS_MAC)
  return new DiagnosticsModelMac();
#elif BUILDFLAG(IS_POSIX)
  return new DiagnosticsModelPosix();
#endif
}

}  // namespace diagnostics
