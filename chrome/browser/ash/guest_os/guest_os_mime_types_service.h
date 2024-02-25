// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_H_

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;
class PrefService;

namespace vm_tools {
namespace apps {
class MimeTypes;
}  // namespace apps
}  // namespace vm_tools

namespace guest_os {

// The GuestOsMimeTypesService stores information about file extension to MIME
// type mappings in Crostini. We store this in prefs so that it is readily
// available even when the VM isn't running.
class GuestOsMimeTypesService : public KeyedService {
 public:
  explicit GuestOsMimeTypesService(Profile* profile);
  GuestOsMimeTypesService(const GuestOsMimeTypesService&) = delete;
  GuestOsMimeTypesService& operator=(const GuestOsMimeTypesService&) = delete;
  ~GuestOsMimeTypesService() override;

  // Returns a MIME type that corresponds to the file extension for the passed
  // in |file_path| for the specified |vm_name| and |container_name|. Returns
  // the empty string if there is no mapping.
  std::string GetMimeType(const base::FilePath& file_path,
                          const std::string& vm_name,
                          const std::string& container_name) const;

  // Returns a vector of extension types that correspond to items of the
  // given |mime_types| for the specified |vm_name| and |container_name|.
  // Returns an empty vector if there is no mapping.
  std::vector<std::string> GetExtensionTypesFromMimeTypes(
      const std::set<std::string>& mime_types,
      const std::string& vm_name,
      const std::string& container_name) const;

  // Remove all MIME type associations for the named VM and container. If
  // |container_name| is empty, all mappings for |vm_name| are removed. Used in
  // the uninstall process.
  void ClearMimeTypes(const std::string& vm_name,
                      const std::string& container_name);

  // The existing list of MIME type mappings is replaced by
  // |mime_type_mappings|.
  void UpdateMimeTypes(const vm_tools::apps::MimeTypes& mime_type_mappings);

 private:
  void UpdateOverrideMimeTypes(std::string vm_name,
                               std::string container_name,
                               base::Value::Dict overrides);

  // Owned by the Profile.
  const raw_ptr<PrefService> prefs_;

  base::WeakPtrFactory<GuestOsMimeTypesService> weak_ptr_factory_{this};
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_MIME_TYPES_SERVICE_H_
