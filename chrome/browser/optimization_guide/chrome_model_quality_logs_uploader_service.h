// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"

class PrefService;
namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {

class ModelExecutionFeaturesController;
class MqlsFeatureMetadata;

class ChromeModelQualityLogsUploaderService
    : public ModelQualityLogsUploaderService {
 public:
  ChromeModelQualityLogsUploaderService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      base::WeakPtr<ModelExecutionFeaturesController>
          model_execution_features_controller);

  ~ChromeModelQualityLogsUploaderService() override;

  ChromeModelQualityLogsUploaderService(
      const ChromeModelQualityLogsUploaderService&) = delete;
  ChromeModelQualityLogsUploaderService& operator=(
      const ChromeModelQualityLogsUploaderService&) = delete;

  // Checks user consent, enterprise check for logging. Returns false if any one
  // of the check is not enabled.
  bool CanUploadLogs(const MqlsFeatureMetadata* metadata) override;

  // Populates the system profile proto and the client's dogfood status.
  void SetSystemMetadata(proto::LoggingMetadata* logging_metadata) override;

 private:
  // This allows checking for enterprise policy on upload.
  base::WeakPtr<ModelExecutionFeaturesController>
      model_execution_feature_controller_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_CHROME_MODEL_QUALITY_LOGS_UPLOADER_SERVICE_H_
