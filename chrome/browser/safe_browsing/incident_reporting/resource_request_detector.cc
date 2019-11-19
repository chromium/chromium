// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/resource_request_detector.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_receiver.h"
#include "chrome/browser/safe_browsing/incident_reporting/resource_request_incident.h"
#include "components/safe_browsing/proto/csd.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "crypto/sha2.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace {

Profile* GetProfileForRenderProcessId(int render_process_id) {
  // How to get a profile from a RenderProcess id:
  // 1) Get the RenderProcessHost
  // 2) From 1) Get the BrowserContext
  // 3) From 2) Get the Profile.
  Profile* profile = nullptr;
  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(render_process_id);
  if (render_process_host) {
    content::BrowserContext* browser_context =
        render_process_host->GetBrowserContext();
    if (browser_context)
      profile = Profile::FromBrowserContext(browser_context);
  }
  return profile;
}

GURL GetUrlForRenderFrameId(int render_process_id, int render_frame_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (render_frame_host)
    return render_frame_host->GetLastCommittedURL();
  return GURL();
}

}  // namespace

namespace safe_browsing {

namespace {

// Implementation of SafeBrowsingDatabaseManager::Client that is used to lookup
// a resource blacklist. Can be constructed on any thread.
class ResourceRequestDetectorClient
    : public SafeBrowsingDatabaseManager::Client,
      public base::RefCountedThreadSafe<ResourceRequestDetectorClient> {
 public:
  using ResourceRequestIncidentMessage =
      ClientIncidentReport::IncidentData::ResourceRequestIncident;

  using OnResultCallback =
      base::OnceCallback<void(std::unique_ptr<ResourceRequestIncidentMessage>)>;

  static void Start(
      const GURL& resource_url,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      OnResultCallback callback) {
    auto client = base::WrapRefCounted(new ResourceRequestDetectorClient(
        std::move(database_manager), std::move(callback)));
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(&ResourceRequestDetectorClient::StartCheck,
                                  client, resource_url));
  }

 private:
  ResourceRequestDetectorClient(
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      OnResultCallback callback)
      : database_manager_(std::move(database_manager)),
        callback_(std::move(callback)) {}

  friend class base::RefCountedThreadSafe<ResourceRequestDetectorClient>;
  ~ResourceRequestDetectorClient() override {}

  void StartCheck(const GURL& resource_url) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (!database_manager_)
      return;

    AddRef();
    // If database_manager_->CheckResourceUrl returns false, the resource might
    // be blacklisted and the response will come in OnCheckResourceUrlResult
    // callback, where AddRef() is balanced by Release().
    // If the check returns true, the resource is not blacklisted and the
    // client object may be destroyed immediately.
    if (database_manager_->CheckResourceUrl(resource_url, this)) {
      Release();
    }
  }

  void OnCheckResourceUrlResult(const GURL& url,
                                SBThreatType threat_type,
                                const std::string& threat_hash) override {
    if (threat_type == SB_THREAT_TYPE_BLACKLISTED_RESOURCE) {
      std::unique_ptr<ResourceRequestIncidentMessage> incident_data(
          new ResourceRequestIncidentMessage());
      incident_data->set_type(ResourceRequestIncidentMessage::TYPE_PATTERN);
      incident_data->set_digest(threat_hash);
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(std::move(callback_), std::move(incident_data)));
    }
    Release();  // Balanced in StartCheck.
  }

  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  OnResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestDetectorClient);
};

}  // namespace

ResourceRequestDetector::ResourceRequestDetector(
    scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
    std::unique_ptr<IncidentReceiver> incident_receiver)
    : incident_receiver_(std::move(incident_receiver)),
      database_manager_(database_manager),
      allow_null_profile_for_testing_(false) {}

ResourceRequestDetector::~ResourceRequestDetector() {
}

void ResourceRequestDetector::ProcessResourceRequest(
    const ResourceRequestInfo* request) {
  // Only look at actual net requests (e.g., not chrome-extensions://id/foo.js).
  if (!request->url.SchemeIsHTTPOrHTTPS())
    return;

  if (request->resource_type == content::ResourceType::kSubFrame ||
      request->resource_type == content::ResourceType::kScript ||
      request->resource_type == content::ResourceType::kObject) {
    ResourceRequestDetectorClient::Start(
        request->url, database_manager_,
        base::BindOnce(&ResourceRequestDetector::ReportIncidentOnUIThread,
                       weak_ptr_factory_.GetWeakPtr(),
                       request->render_process_id, request->render_frame_id));
  }
}

void ResourceRequestDetector::set_allow_null_profile_for_testing(
    bool allow_null_profile_for_testing) {
  allow_null_profile_for_testing_ = allow_null_profile_for_testing;
}

void ResourceRequestDetector::ReportIncidentOnUIThread(
    int render_process_id,
    int render_frame_id,
    std::unique_ptr<ClientIncidentReport_IncidentData_ResourceRequestIncident>
        incident_data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Profile* profile = GetProfileForRenderProcessId(render_process_id);
  if (profile || allow_null_profile_for_testing_) {
    // Add the URL obtained from the RenderFrameHost, if available.
    GURL host_url = GetUrlForRenderFrameId(render_process_id, render_frame_id);
    if (host_url.is_valid())
      incident_data->set_origin(host_url.GetOrigin().spec());

    incident_receiver_->AddIncidentForProfile(
        profile,
        std::make_unique<ResourceRequestIncident>(std::move(incident_data)));
  }
}

}  // namespace safe_browsing
