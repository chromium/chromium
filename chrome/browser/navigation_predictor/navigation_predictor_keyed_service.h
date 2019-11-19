// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

// Keyed service that can be used to receive notifications about the URLs for
// the next predicted navigation.
class NavigationPredictorKeyedService : public KeyedService {
 public:
  // Stores the next set of URLs that the user is expected to navigate to.
  class Prediction {
   public:
    Prediction(const content::RenderFrameHost* render_frame_host,
               const GURL& source_document_url,
               const std::vector<GURL>& sorted_predicted_urls);
    Prediction(const Prediction& other);
    Prediction& operator=(const Prediction& other);
    ~Prediction();
    GURL source_document_url() const;
    std::vector<GURL> sorted_predicted_urls() const;

   private:
    // |render_frame_host_| from where the navigation may happen. May be
    // nullptr.
    const content::RenderFrameHost* render_frame_host_;

    // Current URL of the document from where the navigtion may happen.
    GURL source_document_url_;

    // Ordered set of URLs that the user is expected to navigate to next. The
    // URLs are in the decreasing order of click probability.
    std::vector<GURL> sorted_predicted_urls_;
  };

  // Observer class can be implemented to receive notifications about the
  // predicted URLs for the next navigation. OnPredictionUpdated() is called
  // every time a new prediction is available. Prediction includes the source
  // document as well as the ordered list of URLs that the user may navigate to
  // next. OnPredictionUpdated() may be called multiple times for the same
  // source document URL.
  class Observer {
   public:
    virtual void OnPredictionUpdated(
        const base::Optional<Prediction>& prediction) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  explicit NavigationPredictorKeyedService(
      content::BrowserContext* browser_context);
  ~NavigationPredictorKeyedService() override;

  // |document_url| may be invalid. Called by navigation predictor.
  void OnPredictionUpdated(const content::RenderFrameHost* render_frame_host,
                           const GURL& document_url,
                           const std::vector<GURL>& sorted_predicted_urls);

  // Adds |observer| as the observer for next predicted navigation. When
  // |observer| is added via AddObserver, it's immediately notified of the last
  // known prediction.
  void AddObserver(Observer* observer);

  // Removes |observer| as the observer for next predicted navigation.
  void RemoveObserver(Observer* observer);

 private:
  // List of observers are currently registered to receive notifications for the
  // next predicted navigations.
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Last known prediction.
  base::Optional<Prediction> last_prediction_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorKeyedService);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
