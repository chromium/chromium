// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/common/importer/importer_data_types.h"

// ImporterList detects installed browsers and profiles via
// DetectSourceProfilesWorker(). ImporterList lives on the UI thread.
class ImporterList {
 public:
  ImporterList();

  ImporterList(const ImporterList&) = delete;
  ImporterList& operator=(const ImporterList&) = delete;

  ~ImporterList();

  // Detects the installed browsers and their associated profiles, then stores
  // their information in a list to be accessed via count() and
  // GetSourceProfileAt(). The detection runs asynchronously.
  //
  // |locale|: As in DetectSourceProfilesWorker().
  // |include_interactive_profiles|: True to include source profiles that
  // require user interaction to read.
  // |profiles_loaded_callback|: Assuming this ImporterList instance is still
  // alive, run the callback when the source profile detection finishes.
  void DetectSourceProfiles(const std::string& locale,
                            bool include_interactive_profiles,
                            base::OnceClosure profiles_loaded_callback);

  // Returns the number of different source profiles you can import from.
  size_t count() const { return source_profiles_.size(); }

  // Returns the SourceProfile at |index|. The profiles are ordered such that
  // the profile at index 0 is the likely default browser. The SourceProfile
  // should be passed to ImporterHost::StartImportSettings().
  const importer::SourceProfile& GetSourceProfileAt(size_t index) const;

 private:
  // Called when the source profiles are loaded. Copies the loaded profiles
  // in |profiles| and calls |profiles_loaded_callback|.
  void SourceProfilesLoaded(
      base::OnceClosure profiles_loaded_callback,
      const std::vector<importer::SourceProfile>& profiles);

  // The list of profiles with the default one first.
  std::vector<importer::SourceProfile> source_profiles_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ImporterList> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
