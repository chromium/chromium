// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/mirroring_service_host.h"

#include "content/public/browser/browser_thread.h"

namespace mirroring {

base::WeakPtr<MirroringServiceHost> MirroringServiceHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

MirroringServiceHost::MirroringServiceHost() = default;

MirroringServiceHost::~MirroringServiceHost() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MirroringServiceHostFactory::MirroringServiceHostFactory() = default;

MirroringServiceHostFactory::~MirroringServiceHostFactory() = default;

}  // namespace mirroring
