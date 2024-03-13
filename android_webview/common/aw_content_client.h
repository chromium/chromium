// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_
#define ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_

#include <string_view>

#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/common/content_client.h"

namespace embedder_support {
class OriginTrialPolicyImpl;
}

namespace gpu {
struct GPUInfo;
}

namespace android_webview {

class AwContentClient : public content::ContentClient {
 public:
  AwContentClient();
  ~AwContentClient() override;
  // ContentClient implementation.
  void AddAdditionalSchemes(Schemes* schemes) override;
  std::u16string GetLocalizedString(int message_id) override;
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
  void SetGpuInfo(const gpu::GPUInfo& gpu_info) override;
  void AddContentDecryptionModules(
      std::vector<content::CdmInfo>* cdms,
      std::vector<media::CdmHostFilePath>* cdm_host_file_paths) override;
  bool UsingSynchronousCompositing() override;
  media::MediaDrmBridgeClient* GetMediaDrmBridgeClient() override;
  void ExposeInterfacesToBrowser(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      mojo::BinderMap* binders) override;
  blink::OriginTrialPolicy* GetOriginTrialPolicy() override;

 private:
  // Used to lock when |origin_trial_policy_| is initialized.
  base::Lock origin_trial_policy_lock_;
  std::unique_ptr<embedder_support::OriginTrialPolicyImpl> origin_trial_policy_;
};

bool IsDisableOriginTrialsSafeModeActionOn();

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_COMMON_AW_CONTENT_CLIENT_H_
