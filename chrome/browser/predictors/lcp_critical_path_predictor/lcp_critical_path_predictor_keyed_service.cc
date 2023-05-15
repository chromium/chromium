// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_keyed_service.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/predictors/lcp_critical_path_predictor/lcp_critical_path_predictor_persister.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace {

const base::FilePath::CharType kLCPCriticalPathPredictorKeyedServiceName[] =
    FILE_PATH_LITERAL("LCPCriticalPathPredictor");
}  // namespace

LCPCriticalPathPredictorKeyedService::~LCPCriticalPathPredictorKeyedService() =
    default;

void LCPCriticalPathPredictorKeyedService::Shutdown() {
  // TODO(crbug.com/1419756): Re-visit this to shutdown `persister_` gracefully.
}

bool LCPCriticalPathPredictorKeyedService::IsReady() const {
  return persister_.get();
}

absl::optional<LCPElement> LCPCriticalPathPredictorKeyedService::GetLCPElement(
    const GURL& page_url) {
  return persister_->GetLCPElement(page_url);
}

void LCPCriticalPathPredictorKeyedService::SetLCPElement(
    const GURL& page_url,
    const LCPElement& lcp_element) {
  persister_->SetLCPElement(page_url, lcp_element);
}

LCPCriticalPathPredictorKeyedService::LCPCriticalPathPredictorKeyedService(
    Profile* profile,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner) {
  LCPCriticalPathPredictorPersister::CreateForFilePath(
      std::move(db_task_runner),
      // Backend database is created with this KeyedService name in a profile
      // directory.
      profile->GetPath().Append(kLCPCriticalPathPredictorKeyedServiceName),
      /*flush_delay_for_writes=*/base::TimeDelta(),
      base::BindOnce(&LCPCriticalPathPredictorKeyedService::OnPersisterCreated,
                     weak_factory_.GetWeakPtr()));
}

void LCPCriticalPathPredictorKeyedService::OnPersisterCreated(
    std::unique_ptr<LCPCriticalPathPredictorPersister> persister) {
  CHECK(!persister_);
  persister_ = std::move(persister);
}
