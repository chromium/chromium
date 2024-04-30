// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/tpcd_metadata_component_installer.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "components/component_updater/installer_policies/tpcd_metadata_component_installer_policy.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "content/public/browser/browser_thread.h"

namespace component_updater {

void RegisterTpcdMetadataComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Third Party Cookie Deprecation Metadata component.";

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<TpcdMetadataComponentInstallerPolicy>(
          base::BindRepeating([](const std::string& raw_metadata) {
            tpcd::metadata::Parser::GetInstance()->ParseMetadata(raw_metadata);
          })));

  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
