// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class PrefService;

namespace vm_tools {
namespace apps {
class MimeTypes;
}  // namespace apps
}  // namespace vm_tools

namespace crostini {

// The CrostiniMimeTypesService stores information about file extension to MIME
// type mappings in Crostini. We store this in prefs so that it is readily
// available even when the VM isn't running.
class CrostiniMimeTypesService : public KeyedService {
 public:
  explicit CrostiniMimeTypesService(Profile* profile);
  ~CrostiniMimeTypesService() override;

  // Returns a MIME type that corresponds to the file extension for the passed
  // in |file_path| for the specified |vm_name| and |container_name|. Returns
  // the empty string if there is no mapping.
  std::string GetMimeType(const base::FilePath& file_path,
                          const std::string& vm_name,
                          const std::string& container_name) const;

  // Remove all MIME type associations for the named VM. Used in the
  // uninstall process.
  void ClearMimeTypes(const std::string& vm_name,
                      const std::string& container_name);

  // The existing list of MIME type mappings is replaced by
  // |mime_type_mappings|.
  void UpdateMimeTypes(const vm_tools::apps::MimeTypes& mime_type_mappings);

 private:
  // Owned by the Profile.
  PrefService* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniMimeTypesService);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MIME_TYPES_SERVICE_H_
