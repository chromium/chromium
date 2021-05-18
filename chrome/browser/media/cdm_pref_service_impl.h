// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_IMPL_H_
#define CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_IMPL_H_

#include "content/public/browser/frame_service_base.h"
#include "media/mojo/mojom/cdm_pref_service.mojom.h"
#include "url/origin.h"

class PrefRegistrySimple;

// Manages reads and writes to the user prefs service related to CDM usage.
// Updates to the CDM Origin ID dictionary will be infrequent (ie. every time
// the Media Foundation CDM is used for a new origin). Origin ID are only stored
// for origins serving hardware security protected contents and as such the size
// of the CDM Origin ID dictionary should only contain a handful of items.
class CdmPrefServiceImpl final
    : public content::FrameServiceBase<media::mojom::CdmPrefService> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::CdmPrefService> receiver);

  CdmPrefServiceImpl(const CdmPrefServiceImpl&) = delete;
  CdmPrefServiceImpl& operator=(const CdmPrefServiceImpl&) = delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // media::mojom::CdmPrefService implementation
  void GetCdmOriginId(GetCdmOriginIdCallback callback) override;

 private:
  CdmPrefServiceImpl(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<media::mojom::CdmPrefService> receiver);
  // `this` can only be destructed as a FrameServiceBase
  ~CdmPrefServiceImpl() final;
};

#endif  // CHROME_BROWSER_MEDIA_CDM_PREF_SERVICE_IMPL_H_
