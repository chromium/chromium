// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace component_updater {
class CrOSComponentManager;
}  // namespace component_updater

namespace crosapi {

class LacrosSelectionLoader;
using browser_util::LacrosSelection;

// Manages download of the lacros-chrome binary.
// This class is a part of ash-chrome.
class BrowserLoader {
 public:
  // Constructor for production.
  explicit BrowserLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);
  // Constructor for testing.
  explicit BrowserLoader(
      std::unique_ptr<LacrosSelectionLoader> rootfs_lacros_loader,
      std::unique_ptr<LacrosSelectionLoader> stateful_lacros_loader);

  BrowserLoader(const BrowserLoader&) = delete;
  BrowserLoader& operator=(const BrowserLoader&) = delete;

  virtual ~BrowserLoader();

  // Returns true if the browser loader will try to load stateful lacros-chrome
  // builds from the component manager. This may return false if the user
  // specifies the lacros-chrome binary on the command line or the user has
  // forced the lacros selection to rootfs.
  // If this returns false subsequent loads of lacros-chrome will never load
  // a newer lacros-chrome version and update checking can be skipped.
  static bool WillLoadStatefulComponentBuilds();

  struct LacrosSelectionVersion {
    LacrosSelectionVersion(LacrosSelection selection, base::Version version);

    // LacrosSelection::kRootfs or kStateful should be set here, not
    // kDeployedLocally.
    LacrosSelection selection;

    base::Version version;
  };

  // Indicates how lacros is selected.
  enum class LacrosSelectionSource {
    kUnknown,

    // Selected by comparing the version to choose newer one or either of the
    // lacros selection is not available.
    kCompatibilityCheck,

    // Forced by the selection policy.
    kPolicy,

    // Forced by registered lacros-chrome path.
    kDeployedPath,
  };

  // Starts to load lacros-chrome binary or the rootfs lacros-chrome binary.
  // |callback| is called on completion with the path to the lacros-chrome on
  // success, or an empty filepath on failure, and the loaded lacros selection
  // which is either 'rootfs' or 'stateful'.
  using LoadCompletionCallback = base::OnceCallback<
      void(const base::FilePath&, LacrosSelection, base::Version)>;
  virtual void Load(LoadCompletionCallback callback);

  // Starts to unload lacros-chrome binary.
  // Note that this triggers to remove the user directory for lacros-chrome.
  virtual void Unload();

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadSelectionQuicklyChooseRootfs);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionNeitherIsAvailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionStatefulIsUnavailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionRootfsIsUnavailable);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionRootfsIsNewer);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionRootfsIsOlder);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest,
                           OnLoadVersionSelectionSameVersions);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest, OnLoadSelectionPolicyIsRootfs);
  FRIEND_TEST_ALL_PREFIXES(
      BrowserLoaderTest,
      OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsRootfs);
  FRIEND_TEST_ALL_PREFIXES(
      BrowserLoaderTest,
      OnLoadSelectionPolicyIsUserChoiceAndCommandLineIsStateful);
  FRIEND_TEST_ALL_PREFIXES(BrowserLoaderTest, OnLoadLacrosSpecifiedBySwitch);

  // `source` indicates why rootfs/stateful is selected. `source` is only used
  // for logging.
  void SelectRootfsLacros(LoadCompletionCallback callback,
                          LacrosSelectionSource source);
  void SelectStatefulLacros(LoadCompletionCallback callback,
                            LacrosSelectionSource source);

  // Called on GetVersion for each rootfs and stateful lacros loader.
  void OnGetVersion(
      LacrosSelection selection,
      base::OnceCallback<void(LacrosSelectionVersion)> barrier_callback,
      const base::Version& version);

  // Called to determine which lacros to load based on version (rootfs vs
  // stateful).
  // `versions` consists of 2 elements, one for rootfs lacros, one for stateful
  // lacros. The order is not specified.
  void OnLoadVersions(LoadCompletionCallback callback,
                      std::vector<LacrosSelectionVersion> versions);

  // Called on the completion of loading.
  void OnLoadComplete(LoadCompletionCallback callback,
                      LacrosSelection selection,
                      base::Version version,
                      const base::FilePath& path);
  void FinishOnLoadComplete(LoadCompletionCallback callback,
                            const base::FilePath& path,
                            LacrosSelection selection,
                            base::Version version,
                            bool lacros_binary_exists);

  // Loader for rootfs lacros and stateful lacros.
  std::unique_ptr<LacrosSelectionLoader> rootfs_lacros_loader_;
  std::unique_ptr<LacrosSelectionLoader> stateful_lacros_loader_;

  // Time when the lacros component was loaded.
  base::TimeTicks lacros_start_load_time_;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserLoader> weak_factory_{this};
};

std::ostream& operator<<(std::ostream&, BrowserLoader::LacrosSelectionSource);

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
