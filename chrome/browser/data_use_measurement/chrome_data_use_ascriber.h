// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_set>
#include <utility>

#include "base/feature_list.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/data_use_measurement/chrome_data_use_recorder.h"
#include "components/data_use_measurement/core/data_use_ascriber.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/global_routing_id.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace data_use_measurement {

// Disables data use ascriber if data saver is disabled.
extern const base::Feature kDisableAscriberIfDataSaverDisabled;

class URLRequestClassifier;

// Browser implementation of DataUseAscriber. Maintains a list of
// DataUseRecorder instances, one for each source of data, such as a page
// load.
//
// A page includes all resources loaded in response to a main page navigation.
// The scope of a page load ends either when the tab is closed or a new page
// load is initiated by clicking on a link, entering a new URL, window location
// change, etc.
//
// For URLRequests initiated outside the context of a page load, such as
// Service Workers, Chrome Services, etc, a new instance of DataUseRecorder
// will be created for each URLRequest.
//
// Each page load will be associated with an instance of DataUseRecorder.
// Each URLRequest initiated in the context of a page load will be mapped to
// the DataUseRecorder instance for page load.
//
// This class lives entirely on the IO thread. It maintains a copy of frame and
// navigation information on the IO thread.
class ChromeDataUseAscriber : public DataUseAscriber {
 public:
  ChromeDataUseAscriber();

  ~ChromeDataUseAscriber() override;

  // DataUseAscriber implementation:
  ChromeDataUseRecorder* GetOrCreateDataUseRecorder(
      net::URLRequest* request) override;
  ChromeDataUseRecorder* GetDataUseRecorder(
      const net::URLRequest& request) override;

  void OnBeforeUrlRequest(net::URLRequest* request) override;
  void OnUrlRequestCompleted(net::URLRequest* request, bool started) override;
  void OnUrlRequestDestroyed(net::URLRequest* request) override;
  std::unique_ptr<net::NetworkDelegate> CreateNetworkDelegate(
      std::unique_ptr<net::NetworkDelegate> wrapped_network_delegate) override;
  std::unique_ptr<URLRequestClassifier> CreateURLRequestClassifier()
      const override;

  // Called when a render frame host is created. When the render frame is a main
  // frame, |main_render_process_id| and |main_render_frame_id| should be -1.
  void RenderFrameCreated(int render_process_id,
                          int render_frame_id,
                          int main_render_process_id,
                          int main_render_frame_id);

  // Called when a render frame host is deleted. When the render frame is a main
  // frame, |main_render_process_id| and |main_render_frame_id| should be -1.
  void RenderFrameDeleted(int render_process_id,
                          int render_frame_id,
                          int main_render_process_id,
                          int main_render_frame_id);

  // Called when a main frame navigation is ready to be committed in a
  // renderer.
  void ReadyToCommitMainFrameNavigation(
      content::GlobalRequestID global_request_id,
      int render_process_id,
      int render_frame_id);

  // Called when the main frame navigation is committed in the renderer.
  void DidFinishMainFrameNavigation(int render_process_id,
                                    int render_frame_id,
                                    const GURL& gurl,
                                    bool is_same_document_navigation,
                                    uint32_t page_transition,
                                    base::TimeTicks time);

  // Called every time the WebContents changes visibility.
  void WasShownOrHidden(int main_render_process_id,
                        int main_render_frame_id,
                        bool visible);

  // Called whenever one of the render frames of a WebContents is swapped.
  void RenderFrameHostChanged(int old_render_process_id,
                              int old_render_frame_id,
                              int new_render_process_id,
                              int new_render_frame_id);

  // Called when the load is finished.
  void DidFinishLoad(int render_process_id,
                     int render_frame_id,
                     const GURL& validated_url);

 private:
  friend class ChromeDataUseAscriberTest;

  void OnUrlRequestCompletedOrDestroyed(net::URLRequest* request);

  // Entry in the |data_use_recorders_| list which owns all instances of
  // DataUseRecorder. std::list is used so that iterators remain valid until the
  // lifetime of the container, and will not be invalidated when container is
  // modified.
  typedef std::list<ChromeDataUseRecorder> DataUseRecorderList;
  typedef DataUseRecorderList::iterator DataUseRecorderEntry;

  class DataUseRecorderEntryAsUserData : public base::SupportsUserData::Data {
   public:
    explicit DataUseRecorderEntryAsUserData(DataUseRecorderEntry entry);

    ~DataUseRecorderEntryAsUserData() override;

    DataUseRecorderEntry recorder_entry() { return entry_; }

    static const void* const kDataUseAscriberUserDataKey;

   private:
    DataUseRecorderEntry entry_;
  };

  // Contains the details of a main render frame.
  struct MainRenderFrameEntry {
    explicit MainRenderFrameEntry(
        ChromeDataUseAscriber::DataUseRecorderEntry data_use_recorder);

    ~MainRenderFrameEntry();

    // DataUseRecorderEntry in |data_use_recorders_| that the main frame
    // ascribes data use to.
    DataUseRecorderEntry data_use_recorder;

    // Global requestid of the pending navigation in the main frame, if any.
    // This is needed to support navigations that transfer from one mainframe to
    // another.
    content::GlobalRequestID pending_navigation_global_request_id;

    // Visibility of the main frame, whether it is currently shown to the user.
    // This is updated on WasShownOrHidden(), and passed down to DataUseRecorder
    // when created. Visibility is used to record data use brokend by tab state
    // histograms.
    bool is_visible;

   private:
    DISALLOW_COPY_AND_ASSIGN(MainRenderFrameEntry);
  };

  DataUseRecorderEntry GetDataUseRecorderEntry(const net::URLRequest* request);

  // Validate and cleanup the URL requests that point to |entry|.
  void ValidateAndCleanUp(DataUseRecorderEntry entry);

  DataUseRecorderEntry GetOrCreateDataUseRecorderEntry(
      net::URLRequest* request);

  void NotifyPageLoadCommit(DataUseRecorderEntry entry);
  void NotifyDidFinishLoad(DataUseRecorderEntry entry);
  void NotifyPageLoadConcluded(DataUseRecorderEntry entry);

  DataUseRecorderEntry CreateNewDataUseRecorder(
      net::URLRequest* request,
      DataUse::TrafficType traffic_type);

  bool IsRecorderInPendingNavigationMap(net::URLRequest* request);

  bool IsRecorderInRenderFrameMap(net::URLRequest* request);

  void AscribeRecorderWithRequest(net::URLRequest* request,
                                  DataUseRecorderEntry entry);

  void DisableAscriber() override;

  // Returns true if data use ascriber is disabled.
  bool IsDisabled() const;

  // Owner for all instances of DataUseRecorder. An instance is kept in this
  // list if any entity (render frame hosts, URLRequests, pending navigations)
  // that ascribe data use to the instance exists, and deleted when all
  // ascribing entities go away.
  DataUseRecorderList data_use_recorders_;

  // Map from RenderFrameHost to the MainRenderFrameEntry which contains all
  // details of the main frame. New entry is added on main render frame creation
  // and removed on its deletion.
  std::map<content::GlobalFrameRoutingId, MainRenderFrameEntry>
      main_render_frame_entry_map_;

  // Maps subframe IDs to the mainframe ID, so the mainframe lifetime can have
  // ownership over the lifetime of entries in |data_use_recorders_|. Mainframes
  // are mapped to themselves.
  std::map<content::GlobalFrameRoutingId, content::GlobalFrameRoutingId>
      subframe_to_mainframe_map_;

  // Map from pending navigations to the DataUseRecorderEntry in
  // |data_use_recorders_| that the navigation ascribes data use to.
  std::map<content::GlobalRequestID, DataUseRecorderEntry>
      pending_navigation_data_use_map_;

  // Detects heavy pages. Can be null when the feature is disabled.
  std::unique_ptr<DataUseAscriber::PageLoadObserver> page_capping_observer_;

  // True if the data use ascriber should be disabled. The ascriber is disabled
  // by default.
  bool disable_ascriber_ = true;

  // Set of requests that are currently in-flight.
  std::unordered_set<const net::URLRequest*> requests_;

  DISALLOW_COPY_AND_ASSIGN(ChromeDataUseAscriber);
};

}  // namespace data_use_measurement

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_ASCRIBER_H_
