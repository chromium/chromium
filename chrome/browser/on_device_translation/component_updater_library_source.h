// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATER_LIBRARY_SOURCE_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATER_LIBRARY_SOURCE_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/on_device_translation/library_source.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/update_client/update_client_errors.h"

class PrefService;

namespace on_device_translation {

// Downloads the TranslateKit library as a component with the component
// updater. The component installer stores the path of the installed library in
// a local state pref.
class ComponentUpdaterLibrarySource : public TranslateKitLibrarySource {
 public:
  explicit ComponentUpdaterLibrarySource(PrefService* local_state);
  ~ComponentUpdaterLibrarySource() override;

  // TranslateKitLibrarySource:
  base::FilePath GetLibraryPath() const override;
  void Install() override;

 private:
  void UpdateOnDemand();
  void OnUpdateFinished(update_client::Error error);
  void OnLibraryPathPrefChanged();

  PrefChangeRegistrar pref_change_registrar_;
  // Whether an on-demand update that `Install()` started is in progress.
  bool update_in_progress_ = false;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ComponentUpdaterLibrarySource> weak_ptr_factory_{this};
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_COMPONENT_UPDATER_LIBRARY_SOURCE_H_
