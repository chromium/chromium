// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"

#include <string>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/nqe/network_qualities_prefs_manager.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/url_request/url_request_context.h"

namespace {

// PrefDelegateImpl writes the provided dictionary value to the network quality
// estimator prefs on the disk.
class PrefDelegateImpl
    : public net::NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  // |pref_service| is used to read and write prefs from/to the disk.
  explicit PrefDelegateImpl(PrefService* pref_service)
      : pref_service_(pref_service), path_(prefs::kNetworkQualities) {
    DCHECK(pref_service_);
  }
  ~PrefDelegateImpl() override {}

  void SetDictionaryValue(const base::DictionaryValue& value) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    pref_service_->Set(path_, value);
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.WriteCount", 1, 2);
  }

  std::unique_ptr<base::DictionaryValue> GetDictionaryValue() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    UMA_HISTOGRAM_EXACT_LINEAR("NQE.Prefs.ReadCount", 1, 2);
    return pref_service_->GetDictionary(path_)->CreateDeepCopy();
  }

 private:
  PrefService* pref_service_;

  // |path_| is the location of the network quality estimator prefs.
  const std::string path_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrefDelegateImpl);
};

// Initializes |pref_manager| on |io_thread|.
void SetNQEOnIOThread(net::NetworkQualitiesPrefsManager* prefs_manager,
                      IOThread* io_thread) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Avoid null pointer referencing during browser shutdown, or when the network
  // service is running out of process.
  if (!io_thread->globals()->system_request_context ||
      !io_thread->globals()
           ->system_request_context->network_quality_estimator()) {
    return;
  }

  prefs_manager->InitializeOnNetworkThread(
      io_thread->globals()
          ->system_request_context->network_quality_estimator());
}

}  // namespace

UINetworkQualityEstimatorService::UINetworkQualityEstimatorService(
    Profile* profile)
    : weak_factory_(this) {
  DCHECK(profile);
  // If this is running in a context without an IOThread, don't try to create
  // the IO object.
  if (!g_browser_process->io_thread())
    return;
  std::unique_ptr<PrefDelegateImpl> pref_delegate(
      new PrefDelegateImpl(profile->GetPrefs()));
  prefs_manager_ = base::WrapUnique(
      new net::NetworkQualitiesPrefsManager(std::move(pref_delegate)));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&SetNQEOnIOThread, prefs_manager_.get(),
                     g_browser_process->io_thread()));
}

UINetworkQualityEstimatorService::~UINetworkQualityEstimatorService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void UINetworkQualityEstimatorService::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  weak_factory_.InvalidateWeakPtrs();
  if (prefs_manager_) {
    prefs_manager_->ShutdownOnPrefSequence();
    bool deleted = content::BrowserThread::DeleteSoon(
        content::BrowserThread::IO, FROM_HERE, prefs_manager_.release());
    DCHECK(deleted);
    // Silence unused variable warning in release builds.
    (void)deleted;
  }
}

void UINetworkQualityEstimatorService::ClearPrefs() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!prefs_manager_)
    return;
  prefs_manager_->ClearPrefs();
}

// static
void UINetworkQualityEstimatorService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNetworkQualities,
                                   PrefRegistry::LOSSY_PREF);
}

std::map<net::nqe::internal::NetworkID,
         net::nqe::internal::CachedNetworkQuality>
UINetworkQualityEstimatorService::ForceReadPrefsForTesting() const {
  if (!prefs_manager_) {
    return std::map<net::nqe::internal::NetworkID,
                    net::nqe::internal::CachedNetworkQuality>();
  }
  return prefs_manager_->ForceReadPrefsForTesting();
}
