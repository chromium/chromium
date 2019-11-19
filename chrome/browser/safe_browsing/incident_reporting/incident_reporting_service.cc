// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"

#include <math.h>
#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection.h"
#include "chrome/browser/safe_browsing/incident_reporting/extension_data_collection.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader_impl.h"
#include "chrome/browser/safe_browsing/incident_reporting/preference_validation_delegate.h"
#include "chrome/browser/safe_browsing/incident_reporting/state_store.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

namespace safe_browsing {

const base::Feature kIncidentReportingEnableUpload {
  "IncidentReportingEnableUpload",
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      base::FEATURE_ENABLED_BY_DEFAULT
#else
      base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

namespace {

// The action taken for an incident; used for user metrics (see
// LogIncidentDataType).
enum IncidentDisposition {
  RECEIVED,
  DROPPED,
  ACCEPTED,
  PRUNED,
  DISCARDED,
  NO_DOWNLOAD,
  NUM_DISPOSITIONS
};

// The state persisted for a specific instance of an incident to enable pruning
// of previously-reported incidents.
struct PersistentIncidentState {
  // The type of the incident.
  IncidentType type;

  // The key for a specific instance of an incident.
  std::string key;

  // A hash digest representing a specific instance of an incident.
  uint32_t digest;
};

// The amount of time the service will wait to collate incidents.
const int64_t kDefaultUploadDelayMs = 1000 * 60;  // one minute

// The amount of time between running delayed analysis callbacks.
const int64_t kDefaultCallbackIntervalMs = 1000 * 20;

// Logs the type of incident in |incident_data| to a user metrics histogram.
void LogIncidentDataType(IncidentDisposition disposition,
                         const Incident& incident) {
  static const char* const kHistogramNames[] = {
      "SBIRS.ReceivedIncident",
      "SBIRS.DroppedIncident",
      "SBIRS.Incident",
      "SBIRS.PrunedIncident",
      "SBIRS.DiscardedIncident",
      "SBIRS.NoDownloadIncident",
  };
  static_assert(base::size(kHistogramNames) == NUM_DISPOSITIONS,
                "Keep kHistogramNames in sync with enum IncidentDisposition.");
  DCHECK_GE(disposition, 0);
  DCHECK_LT(disposition, NUM_DISPOSITIONS);
  base::LinearHistogram::FactoryGet(
      kHistogramNames[disposition],
      1,  // minimum
      static_cast<int32_t>(IncidentType::NUM_TYPES),  // maximum
      static_cast<size_t>(IncidentType::NUM_TYPES) + 1,  // bucket_count
      base::HistogramBase::kUmaTargetedHistogramFlag)->Add(
          static_cast<int32_t>(incident.GetType()));
}

// Computes the persistent state for an incident.
PersistentIncidentState ComputeIncidentState(const Incident& incident) {
  PersistentIncidentState state = {
    incident.GetType(),
    incident.GetKey(),
    incident.ComputeDigest(),
  };
  return state;
}

// Returns the shutdown behavior for the task runners of the incident reporting
// service. Current metrics suggest that CONTINUE_ON_SHUTDOWN will reduce the
// number of browser hangs on shutdown.
base::TaskShutdownBehavior GetShutdownBehavior() {
  return base::FeatureList::IsEnabled(features::kBrowserHangFixesExperiment)
             ? base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN
             : base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN;
}

// Returns a task runner for blocking tasks in the background.
scoped_refptr<base::TaskRunner> GetBackgroundTaskRunner() {
  return base::CreateTaskRunner({base::ThreadPool(),
                                 base::TaskPriority::BEST_EFFORT,
                                 GetShutdownBehavior(), base::MayBlock()});
}

}  // namespace

struct IncidentReportingService::ProfileContext {
  ProfileContext();
  ~ProfileContext();

  // Returns true if the profile has incidents to be uploaded or cleared.
  bool HasIncidents() const;

  // The incidents collected for this profile pending creation and/or upload.
  // Will contain null values for pruned incidents.
  std::vector<std::unique_ptr<Incident>> incidents;

  // The incidents data of which should be cleared.
  std::vector<std::unique_ptr<Incident>> incidents_to_clear;

  // State storage for this profile; null until OnProfileAdded is called.
  std::unique_ptr<StateStore> state_store;

  // False until OnProfileAdded is called.
  bool added;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProfileContext);
};

class IncidentReportingService::UploadContext {
 public:
  typedef std::map<ProfileContext*, std::vector<PersistentIncidentState>>
      PersistentIncidentStateCollection;

  explicit UploadContext(std::unique_ptr<ClientIncidentReport> report);
  ~UploadContext();

  // The report being uploaded.
  std::unique_ptr<ClientIncidentReport> report;

  // The uploader in use.
  std::unique_ptr<IncidentReportUploader> uploader;

  // A mapping of profile contexts to the data to be persisted upon successful
  // upload.
  PersistentIncidentStateCollection profiles_to_state;

 private:
  DISALLOW_COPY_AND_ASSIGN(UploadContext);
};

// An IncidentReceiver that is weakly-bound to the service and transparently
// bounces process-wide incidents back to the main thread for handling.
class IncidentReportingService::Receiver : public IncidentReceiver {
 public:
  explicit Receiver(const base::WeakPtr<IncidentReportingService>& service);
  ~Receiver() override;

  // IncidentReceiver methods:
  void AddIncidentForProfile(Profile* profile,
                             std::unique_ptr<Incident> incident) override;
  void AddIncidentForProcess(std::unique_ptr<Incident> incident) override;
  void ClearIncidentForProcess(std::unique_ptr<Incident> incident) override;

 private:
  static void AddIncidentOnMainThread(
      const base::WeakPtr<IncidentReportingService>& service,
      Profile* profile,
      std::unique_ptr<Incident> incident);
  static void ClearIncidentOnMainThread(
      const base::WeakPtr<IncidentReportingService>& service,
      Profile* profile,
      std::unique_ptr<Incident> incident);

  base::WeakPtr<IncidentReportingService> service_;
  scoped_refptr<base::SingleThreadTaskRunner> thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(Receiver);
};

IncidentReportingService::Receiver::Receiver(
    const base::WeakPtr<IncidentReportingService>& service)
    : service_(service),
      thread_runner_(base::ThreadTaskRunnerHandle::Get()) {
}

IncidentReportingService::Receiver::~Receiver() {
}

void IncidentReportingService::Receiver::AddIncidentForProfile(
    Profile* profile,
    std::unique_ptr<Incident> incident) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile);
  AddIncidentOnMainThread(service_, profile, std::move(incident));
}

void IncidentReportingService::Receiver::AddIncidentForProcess(
    std::unique_ptr<Incident> incident) {
  if (thread_runner_->BelongsToCurrentThread()) {
    AddIncidentOnMainThread(service_, nullptr, std::move(incident));
  } else {
    thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IncidentReportingService::Receiver::AddIncidentOnMainThread,
            service_, nullptr, std::move(incident)));
  }
}

void IncidentReportingService::Receiver::ClearIncidentForProcess(
    std::unique_ptr<Incident> incident) {
  if (thread_runner_->BelongsToCurrentThread()) {
    ClearIncidentOnMainThread(service_, nullptr, std::move(incident));
  } else {
    thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &IncidentReportingService::Receiver::ClearIncidentOnMainThread,
            service_, nullptr, std::move(incident)));
  }
}

bool IncidentReportingService::HasIncidentsToUpload() const {
  for (const auto& profile_and_context : profiles_) {
    if (!profile_and_context.second->incidents.empty())
      return true;
  }
  return false;
}

// static
void IncidentReportingService::Receiver::AddIncidentOnMainThread(
    const base::WeakPtr<IncidentReportingService>& service,
    Profile* profile,
    std::unique_ptr<Incident> incident) {
  if (service)
    service->AddIncident(profile, std::move(incident));
  else
    LogIncidentDataType(DISCARDED, *incident);
}

// static
void IncidentReportingService::Receiver::ClearIncidentOnMainThread(
    const base::WeakPtr<IncidentReportingService>& service,
    Profile* profile,
    std::unique_ptr<Incident> incident) {
  if (service)
    service->ClearIncident(profile, std::move(incident));
}

IncidentReportingService::ProfileContext::ProfileContext() : added(false) {
}

IncidentReportingService::ProfileContext::~ProfileContext() {
  for (const auto& incident : incidents) {
    if (incident)
      LogIncidentDataType(DISCARDED, *incident);
  }
}

bool IncidentReportingService::ProfileContext::HasIncidents() const {
  return !incidents.empty() || !incidents_to_clear.empty();
}

IncidentReportingService::UploadContext::UploadContext(
    std::unique_ptr<ClientIncidentReport> report)
    : report(std::move(report)) {}

IncidentReportingService::UploadContext::~UploadContext() {
}

// static
bool IncidentReportingService::IsEnabledForProfile(Profile* profile) {
  if (profile->IsOffTheRecord())
    return false;
  if (!profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled))
    return false;
  return IsExtendedReportingEnabled(*profile->GetPrefs());
}

IncidentReportingService::IncidentReportingService(
    SafeBrowsingService* safe_browsing_service)
    : IncidentReportingService(
          safe_browsing_service,
          base::TimeDelta::FromMilliseconds(kDefaultCallbackIntervalMs),
          GetBackgroundTaskRunner()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DownloadProtectionService* download_protection_service =
      (safe_browsing_service
           ? safe_browsing_service->download_protection_service()
           : nullptr);
  if (download_protection_service) {
    client_download_request_subscription_ =
        download_protection_service->RegisterClientDownloadRequestCallback(
            base::Bind(&IncidentReportingService::OnClientDownloadRequest,
                       base::Unretained(this)));
  }
}

IncidentReportingService::~IncidentReportingService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CancelIncidentCollection();

  // Cancel all internal asynchronous tasks.
  weak_ptr_factory_.InvalidateWeakPtrs();

  CancelEnvironmentCollection();
  CancelDownloadCollection();
  CancelAllReportUploads();

  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

std::unique_ptr<IncidentReceiver>
IncidentReportingService::GetIncidentReceiver() {
  return std::make_unique<Receiver>(receiver_weak_ptr_factory_.GetWeakPtr());
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
IncidentReportingService::CreatePreferenceValidationDelegate(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile->IsOffTheRecord())
    return std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>();
  return std::make_unique<PreferenceValidationDelegate>(profile,
                                                        GetIncidentReceiver());
}

void IncidentReportingService::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // |callback| will be run on the blocking pool. The receiver will bounce back
  // to the origin thread if needed.
  delayed_analysis_callbacks_.RegisterCallback(
      base::Bind(callback, base::Passed(GetIncidentReceiver())));

  // Start running the callbacks if any profiles are participating in safe
  // browsing extended reporting. If none are now, running will commence if/when
  // such a profile is added.
  if (FindEligibleProfile())
    delayed_analysis_callbacks_.Start();
}

void IncidentReportingService::AddDownloadManager(
    content::DownloadManager* download_manager) {
  download_metadata_manager_.AddDownloadManager(download_manager);
}

IncidentReportingService::IncidentReportingService(
    SafeBrowsingService* safe_browsing_service,
    base::TimeDelta delayed_task_interval,
    const scoped_refptr<base::TaskRunner>& delayed_task_runner)
    : url_loader_factory_(safe_browsing_service
                              ? safe_browsing_service->GetURLLoaderFactory()
                              : nullptr),
      collect_environment_data_fn_(&CollectEnvironmentData),
      environment_collection_task_runner_(GetBackgroundTaskRunner()),
      collation_timer_(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(kDefaultUploadDelayMs),
                       this,
                       &IncidentReportingService::OnCollationTimeout),
      delayed_analysis_callbacks_(delayed_task_interval, delayed_task_runner) {
  if (g_browser_process->profile_manager())
    g_browser_process->profile_manager()->AddObserver(this);
}

void IncidentReportingService::SetCollectEnvironmentHook(
    CollectEnvironmentDataFn collect_environment_data_hook,
    const scoped_refptr<base::TaskRunner>& task_runner) {
  if (collect_environment_data_hook) {
    collect_environment_data_fn_ = collect_environment_data_hook;
    environment_collection_task_runner_ = task_runner;
  } else {
    collect_environment_data_fn_ = &CollectEnvironmentData;
    environment_collection_task_runner_ = GetBackgroundTaskRunner();
  }
}

void IncidentReportingService::DoExtensionCollection(
    ClientIncidentReport_ExtensionData* extension_data) {
  CollectExtensionData(extension_data);
}

void IncidentReportingService::OnProfileAdded(Profile* profile) {
  // Handle the addition of a new profile to the ProfileManager. Create a new
  // context for |profile| if one does not exist, drop any received incidents
  // for the profile if the profile is not participating in safe browsing
  // extended reporting, and initiate a new search for the most recent download
  // if a report is being assembled and the most recent has not been found.
  // Note that |profile| is assumed to outlive |this|.

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Track the addition of all profiles even when no report is being assembled
  // so that the service can determine whether or not it can evaluate a
  // profile's preferences at the time of incident addition.
  ProfileContext* context = GetOrCreateProfileContext(profile);
  DCHECK(!context->added);
  context->added = true;
  context->state_store = std::make_unique<StateStore>(profile);
  bool enabled_for_profile = IsEnabledForProfile(profile);

  // Drop all incidents associated with this profile that were received prior to
  // its addition if incident reporting is not enabled for it.
  if (!context->incidents.empty() && !enabled_for_profile) {
    for (const auto& incident : context->incidents)
      LogIncidentDataType(DROPPED, *incident);
    context->incidents.clear();
  }

  if (enabled_for_profile) {
    // Start processing delayed analysis callbacks if incident reporting is
    // enabled for this new profile. Start is idempotent, so this is safe even
    // if they're already running.
    delayed_analysis_callbacks_.Start();

    // Start a new report if there are process-wide incidents, or incidents for
    // this profile.
    if ((GetProfileContext(nullptr) &&
         GetProfileContext(nullptr)->HasIncidents()) ||
        context->HasIncidents()) {
      BeginReportProcessing();
    }
  }

  // TODO(grt): register for pref change notifications to start delayed analysis
  // and/or report processing if sb is currently disabled but subsequently
  // enabled.

  // Nothing else to do if a report is not being assembled.
  if (!report_)
    return;

  // Environment collection is deferred until at least one profile for which the
  // service is enabled is added. Re-initiate collection now in case this is the
  // first such profile.
  BeginEnvironmentCollection();
  // Take another stab at finding the most recent download if a report is being
  // assembled and one hasn't been found yet (the LastDownloadFinder operates
  // only on profiles that have been added to the ProfileManager).
  BeginDownloadCollection();
}

std::unique_ptr<LastDownloadFinder>
IncidentReportingService::CreateDownloadFinder(
    const LastDownloadFinder::LastDownloadCallback& callback) {
  return LastDownloadFinder::Create(
      base::Bind(&DownloadMetadataManager::GetDownloadDetails,
                 base::Unretained(&download_metadata_manager_)),
      callback);
}

std::unique_ptr<IncidentReportUploader>
IncidentReportingService::StartReportUpload(
    const IncidentReportUploader::OnResultCallback& callback,
    const ClientIncidentReport& report) {
  return IncidentReportUploaderImpl::UploadReport(callback, url_loader_factory_,
                                                  report);
}

bool IncidentReportingService::IsProcessingReport() const {
  return report_ != nullptr;
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetOrCreateProfileContext(Profile* profile) {
  std::unique_ptr<ProfileContext>& context = profiles_[profile];
  if (!context)
    context = std::make_unique<ProfileContext>();
  return context.get();
}

IncidentReportingService::ProfileContext*
IncidentReportingService::GetProfileContext(Profile* profile) {
  auto it = profiles_.find(profile);
  return it != profiles_.end() ? it->second.get() : nullptr;
}

Profile* IncidentReportingService::FindEligibleProfile() const {
  for (const auto& scan : profiles_) {
    // Skip over profiles that have yet to be added to the profile manager.
    // This will also skip over the NULL-profile context used to hold
    // process-wide incidents.
    if (!scan.second->added)
      continue;

    if (IsEnabledForProfile(scan.first))
      return scan.first;
  }

  return nullptr;
}

void IncidentReportingService::AddIncident(Profile* profile,
                                           std::unique_ptr<Incident> incident) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Ignore incidents from off-the-record profiles.
  if (profile && profile->IsOffTheRecord())
    return;

  ProfileContext* context = GetOrCreateProfileContext(profile);
  // If this is a process-wide incident, the context must not indicate that the
  // profile (which is NULL) has been added to the profile manager.
  DCHECK(profile || !context->added);

  LogIncidentDataType(RECEIVED, *incident);

  // Drop the incident immediately if the profile has already been added to the
  // manager and does not have incident reporting enabled. Preference evaluation
  // is deferred until OnProfileAdded() otherwise.
  if (context->added && !IsEnabledForProfile(profile)) {
    LogIncidentDataType(DROPPED, *incident);
    return;
  }

  // Take ownership of the incident.
  context->incidents.push_back(std::move(incident));

  // Remember when the first incident for this report arrived.
  if (first_incident_time_.is_null())
    first_incident_time_ = base::Time::Now();
  // Log the time between the previous incident and this one.
  if (!last_incident_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("SBIRS.InterIncidentTime",
                        base::TimeTicks::Now() - last_incident_time_);
  }
  last_incident_time_ = base::TimeTicks::Now();

  // Persist the incident data.

  // Start assembling a new report if this is the first incident ever or the
  // first since the last upload.
  BeginReportProcessing();
}

void IncidentReportingService::ClearIncident(
    Profile* profile,
    std::unique_ptr<Incident> incident) {
  ProfileContext* context = GetOrCreateProfileContext(profile);
  context->incidents_to_clear.push_back(std::move(incident));
  // Begin processing to handle cleared incidents following collation.
  BeginReportProcessing();
}

void IncidentReportingService::BeginReportProcessing() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Creates a new report if needed.
  if (!report_)
    report_.reset(new ClientIncidentReport());

  // Ensure that collection tasks are running (calls are idempotent).
  BeginIncidentCollation();
  BeginEnvironmentCollection();
  BeginDownloadCollection();
}

void IncidentReportingService::BeginIncidentCollation() {
  // Restart the delay timer to send the report upon expiration.
  collation_timeout_pending_ = true;
  collation_timer_.Reset();
}

bool IncidentReportingService::WaitingToCollateIncidents() {
  return collation_timeout_pending_;
}

void IncidentReportingService::CancelIncidentCollection() {
  collation_timeout_pending_ = false;
  last_incident_time_ = base::TimeTicks();
  report_.reset();
}

void IncidentReportingService::OnCollationTimeout() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Exit early if collection was cancelled.
  if (!collation_timeout_pending_)
    return;

  // Wait another round if profile-bound incidents have come in from a profile
  // that has yet to complete creation.
  for (const auto& scan : profiles_) {
    if (scan.first && !scan.second->added && scan.second->HasIncidents()) {
      collation_timer_.Reset();
      return;
    }
  }

  collation_timeout_pending_ = false;

  ProcessIncidentsIfCollectionComplete();
}

void IncidentReportingService::BeginEnvironmentCollection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(report_);
  // Nothing to do if environment collection is pending or has already
  // completed, if there are no incidents to process, or if there is no eligible
  // profile.
  if (environment_collection_pending_ || report_->has_environment() ||
      !HasIncidentsToUpload() || !FindEligibleProfile()) {
    return;
  }

  environment_collection_begin_ = base::TimeTicks::Now();
  ClientIncidentReport_EnvironmentData* environment_data =
      new ClientIncidentReport_EnvironmentData();
  environment_collection_pending_ =
      environment_collection_task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(collect_environment_data_fn_, environment_data),
          base::BindOnce(&IncidentReportingService::OnEnvironmentDataCollected,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::WrapUnique(environment_data)));

  // Posting the task will fail if the runner has been shut down. This should
  // never happen since the blocking pool is shut down after this service.
  DCHECK(environment_collection_pending_);
}

bool IncidentReportingService::WaitingForEnvironmentCollection() {
  return environment_collection_pending_;
}

void IncidentReportingService::CancelEnvironmentCollection() {
  environment_collection_begin_ = base::TimeTicks();
  environment_collection_pending_ = false;
  if (report_)
    report_->clear_environment();
}

void IncidentReportingService::OnEnvironmentDataCollected(
    std::unique_ptr<ClientIncidentReport_EnvironmentData> environment_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(environment_collection_pending_);
  DCHECK(report_ && !report_->has_environment());
  environment_collection_pending_ = false;

// Process::Current().CreationTime() is missing on some platforms.
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  base::TimeDelta uptime =
      first_incident_time_ - base::Process::Current().CreationTime();
  environment_data->mutable_process()->set_uptime_msec(uptime.InMilliseconds());
#endif

  report_->set_allocated_environment(environment_data.release());

  UMA_HISTOGRAM_TIMES("SBIRS.EnvCollectionTime",
                      base::TimeTicks::Now() - environment_collection_begin_);
  environment_collection_begin_ = base::TimeTicks();

  ProcessIncidentsIfCollectionComplete();
}

void IncidentReportingService::BeginDownloadCollection() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(report_);
  // Nothing to do if a search for the most recent download is already pending,
  // if one has already been found, or if there are no incidents to process.
  if (last_download_finder_ || report_->has_download() ||
      !HasIncidentsToUpload()) {
    return;
  }

  last_download_begin_ = base::TimeTicks::Now();
  last_download_finder_ = CreateDownloadFinder(
      base::Bind(&IncidentReportingService::OnLastDownloadFound,
                 weak_ptr_factory_.GetWeakPtr()));
  // No instance is returned if there are no eligible loaded profiles. Another
  // search will be attempted in OnProfileAdded() if another profile appears on
  // the scene.
  if (!last_download_finder_)
    last_download_begin_ = base::TimeTicks();
}

bool IncidentReportingService::WaitingForMostRecentDownload() {
  DCHECK(report_);  // Only call this when a report is being assembled.
  // The easy case: not waiting if a download has already been found.
  if (report_->has_download())
    return false;
  // The next easy case: waiting if the finder is operating.
  if (last_download_finder_)
    return true;
  // Harder case 1: not waiting if there are no incidents to upload (only
  // incidents to clear).
  if (!HasIncidentsToUpload())
    return false;
  // Harder case 2: waiting if a non-NULL profile has not yet been added.
  for (const auto& scan : profiles_) {
    if (scan.first && !scan.second->added)
      return true;
  }
  // There is no most recent download and there's nothing more to wait for.
  return false;
}

void IncidentReportingService::CancelDownloadCollection() {
  last_download_finder_.reset();
  last_download_begin_ = base::TimeTicks();
  if (report_)
    report_->clear_download();
}

void IncidentReportingService::OnLastDownloadFound(
    std::unique_ptr<ClientIncidentReport_DownloadDetails> last_binary_download,
    std::unique_ptr<ClientIncidentReport_NonBinaryDownloadDetails>
        last_non_binary_download) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(report_);

  UMA_HISTOGRAM_TIMES("SBIRS.FindDownloadedBinaryTime",
                      base::TimeTicks::Now() - last_download_begin_);
  last_download_begin_ = base::TimeTicks();

  // Harvest the finder.
  last_download_finder_.reset();

  if (last_binary_download)
    report_->set_allocated_download(last_binary_download.release());

  if (last_non_binary_download) {
    report_->set_allocated_non_binary_download(
        last_non_binary_download.release());
  }

  ProcessIncidentsIfCollectionComplete();
}

void IncidentReportingService::ProcessIncidentsIfCollectionComplete() {
  DCHECK(report_);
  // Bail out if there are still outstanding collection tasks. Completion of any
  // of these will start another upload attempt.
  if (WaitingForEnvironmentCollection() || WaitingToCollateIncidents() ||
      WaitingForMostRecentDownload()) {
    return;
  }

  // Take ownership of the report and clear things for future reports.
  std::unique_ptr<ClientIncidentReport> report(std::move(report_));
  first_incident_time_ = base::Time();
  last_incident_time_ = base::TimeTicks();

  ClientIncidentReport_EnvironmentData_Process* process =
      report->mutable_environment()->mutable_process();

  process->set_metrics_consent(
      ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());

  // Associate process-wide incidents with any eligible profile. If there is no
  // eligible profile, drop the incidents.
  ProfileContext* null_context = GetProfileContext(nullptr);
  if (null_context && null_context->HasIncidents()) {
    Profile* eligible_profile = FindEligibleProfile();
    if (eligible_profile) {
      ProfileContext* eligible_context = GetProfileContext(eligible_profile);
      // Move the incidents to the target context.
      for (auto& incident : null_context->incidents) {
        eligible_context->incidents.push_back(std::move(incident));
      }
      null_context->incidents.clear();
      for (auto& incident : null_context->incidents_to_clear)
        eligible_context->incidents_to_clear.push_back(std::move(incident));
      null_context->incidents_to_clear.clear();
    } else {
      for (const auto& incident : null_context->incidents)
        LogIncidentDataType(DROPPED, *incident);
      null_context->incidents.clear();
    }
  }

  // Clear incidents data where needed.
  for (auto& profile_and_context : profiles_) {
    // Bypass process-wide incidents that have not yet been associated with a
    // profile and profiles with no incidents to clear.
    if (!profile_and_context.first ||
        profile_and_context.second->incidents_to_clear.empty()) {
      continue;
    }
    ProfileContext* context = profile_and_context.second.get();
    StateStore::Transaction transaction(context->state_store.get());
    for (const auto& incident : context->incidents_to_clear)
      transaction.Clear(incident->GetType(), incident->GetKey());
    context->incidents_to_clear.clear();
  }
  // Abandon report if there are no incidents to upload.
  if (!HasIncidentsToUpload())
    return;

  bool has_download =
      report->has_download() || report->has_non_binary_download();

  // Collect incidents across all profiles participating in safe browsing
  // extended reporting.
  // Associate the profile contexts and their incident data with the upload.
  UploadContext::PersistentIncidentStateCollection profiles_to_state;
  for (auto& profile_and_context : profiles_) {
    // Bypass process-wide incidents that have not yet been associated with a
    // profile.
    if (!profile_and_context.first)
      continue;
    ProfileContext* context = profile_and_context.second.get();
    if (context->incidents.empty())
      continue;
    // Drop all incidents collected for the profile if it stopped participating
    // before collection completed.
    if (!IsEnabledForProfile(profile_and_context.first)) {
      for (const auto& incident : context->incidents)
        LogIncidentDataType(DROPPED, *incident);
      context->incidents.clear();
      continue;
    }
    StateStore::Transaction transaction(context->state_store.get());
    std::vector<PersistentIncidentState> states;
    // Prep persistent data and prune any incidents already sent.
    for (const auto& incident : context->incidents) {
      const PersistentIncidentState state = ComputeIncidentState(*incident);
      if (context->state_store->HasBeenReported(state.type, state.key,
                                                state.digest)) {
        LogIncidentDataType(PRUNED, *incident);
      } else if (!has_download) {
        LogIncidentDataType(NO_DOWNLOAD, *incident);
        // Drop the incident and mark for future pruning since no executable
        // download was found.
        transaction.MarkAsReported(state.type, state.key, state.digest);
      } else {
        LogIncidentDataType(ACCEPTED, *incident);
        // Ownership of the payload is passed to the report.
        ClientIncidentReport_IncidentData* data =
            incident->TakePayload().release();
        DCHECK(data->has_incident_time_msec());
        report->mutable_incident()->AddAllocated(data);
        data = nullptr;
        states.push_back(state);
      }
    }
    context->incidents.clear();
    profiles_to_state[context].swap(states);
  }

  const int count = report->incident_size();

  // Abandon the request if all incidents were pruned or otherwise dropped.
  if (!count) {
    if (!has_download) {
      UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                                IncidentReportUploader::UPLOAD_NO_DOWNLOAD,
                                IncidentReportUploader::NUM_UPLOAD_RESULTS);
    }
    return;
  }
  // Perform final synchronous collection tasks for the report.
  DoExtensionCollection(report->mutable_extension_data());

  std::unique_ptr<UploadContext> context(new UploadContext(std::move(report)));
  context->profiles_to_state.swap(profiles_to_state);
  UploadContext* temp_context = context.get();
  uploads_.push_back(std::move(context));
  IncidentReportingService::UploadReportIfUploadingEnabled(temp_context);
}

void IncidentReportingService::CancelAllReportUploads() {
  for (size_t i = 0; i < uploads_.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("SBIRS.UploadResult",
                              IncidentReportUploader::UPLOAD_CANCELLED,
                              IncidentReportUploader::NUM_UPLOAD_RESULTS);
  }
  uploads_.clear();
}

void IncidentReportingService::UploadReportIfUploadingEnabled(
    UploadContext* context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!base::FeatureList::IsEnabled(kIncidentReportingEnableUpload)) {
    OnReportUploadResult(context, IncidentReportUploader::UPLOAD_SUPPRESSED,
                         std::unique_ptr<ClientIncidentResponse>());
    return;
  }

  // Initiate the upload.
  context->uploader = StartReportUpload(
      base::Bind(&IncidentReportingService::OnReportUploadResult,
                 weak_ptr_factory_.GetWeakPtr(), context),
      *context->report);
  if (!context->uploader) {
    OnReportUploadResult(context,
                         IncidentReportUploader::UPLOAD_INVALID_REQUEST,
                         std::unique_ptr<ClientIncidentResponse>());
  }
}

void IncidentReportingService::HandleResponse(const UploadContext& context) {
  // Mark each incident as reported in its corresponding profile's state store.
  for (const auto& context_and_states : context.profiles_to_state) {
    StateStore::Transaction transaction(
        context_and_states.first->state_store.get());
    for (const auto& state : context_and_states.second)
      transaction.MarkAsReported(state.type, state.key, state.digest);
  }
}

void IncidentReportingService::OnReportUploadResult(
    UploadContext* context,
    IncidentReportUploader::Result result,
    std::unique_ptr<ClientIncidentResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  UMA_HISTOGRAM_ENUMERATION(
      "SBIRS.UploadResult", result, IncidentReportUploader::NUM_UPLOAD_RESULTS);

  // The upload is no longer outstanding, so take ownership of the context (from
  // the collection of outstanding uploads) in this scope.
  auto it =
      std::find_if(uploads_.begin(), uploads_.end(),
                   [context](const std::unique_ptr<UploadContext>& value) {
                     return value.get() == context;
                   });
  DCHECK(it != uploads_.end());
  std::unique_ptr<UploadContext> upload(std::move(*it));
  uploads_.erase(it);

  if (result == IncidentReportUploader::UPLOAD_SUCCESS)
    HandleResponse(*upload);
  // else retry?
}

void IncidentReportingService::OnClientDownloadRequest(
    download::DownloadItem* download,
    const ClientDownloadRequest* request) {
  if (content::DownloadItemUtils::GetBrowserContext(download) &&
      !content::DownloadItemUtils::GetBrowserContext(download)
           ->IsOffTheRecord()) {
    download_metadata_manager_.SetRequest(download, request);
  }
}

}  // namespace safe_browsing
