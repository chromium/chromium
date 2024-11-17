// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_

#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// Keyed service that can be used to receive notifications about the URLs for
// the next predicted navigation.
class NavigationPredictorKeyedService : public KeyedService {
 public:
  // Indicates how the set of next navigation URLs were predicted.
  enum class PredictionSource {
    // Next navigation URLs were predicted by navigation predictor by parsing
    // the anchor element metrics on a webpage.
    kAnchorElementsParsedFromWebPage = 0,
  };

  // Stores the next set of URLs that the user is expected to navigate to.
  class Prediction {
   public:
    Prediction(content::WebContents* web_contents,
               const std::optional<GURL>& source_document_url,
               PredictionSource prediction_source,
               const std::vector<GURL>& sorted_predicted_urls);
    Prediction(const Prediction& other);
    Prediction& operator=(const Prediction& other);
    ~Prediction();
    const std::optional<GURL>& source_document_url() const;
    PredictionSource prediction_source() const { return prediction_source_; }
    const std::vector<GURL>& sorted_predicted_urls() const;

    // Null if the prediction source is kExternalAndroidApp.
    content::WebContents* web_contents() const;

   private:
    // The WebContents from where the navigation may happen. Do not use this
    // pointer outside the observer's call stack unless its destruction is also
    // observed.
    raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;

    // TODO(spelchat): this no longer needs to be optional. Optionality was
    // required because external app predictions didn't provide this field, but
    // external predictions are no longer supported.
    // Current URL of the document from where the navigtion may happen.
    std::optional<GURL> source_document_url_;

    // |prediction_source_| indicates how the prediction was generated and
    // affects how the prediction should be consumed. If the
    // |prediction_source_| is kAnchorElementsParsedFromWebPage, then
    // |source_document_url_| is the webpage from where the predictions were
    // generated.
    PredictionSource prediction_source_;

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
  //
  // Observers must follow relevant privacy guidelines when consuming the
  // notifications.
  class Observer {
   public:
    virtual void OnPredictionUpdated(const Prediction& prediction) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  explicit NavigationPredictorKeyedService(
      content::BrowserContext* browser_context);

  NavigationPredictorKeyedService(const NavigationPredictorKeyedService&) =
      delete;
  NavigationPredictorKeyedService& operator=(
      const NavigationPredictorKeyedService&) = delete;

  ~NavigationPredictorKeyedService() override;

  SearchEnginePreconnector* search_engine_preconnector();

  // |document_url| may be invalid. Called by navigation predictor.
  void OnPredictionUpdated(content::WebContents* web_contents,
                           const GURL& document_url,
                           PredictionSource prediction_source,
                           const std::vector<GURL>& sorted_predicted_urls);

  // Adds |observer| as the observer for next predicted navigation. When
  // |observer| is added via AddObserver, it's immediately notified of the last
  // known prediction. Observers must follow relevant privacy guidelines when
  // consuming the notifications.
  void AddObserver(Observer* observer);

  // Removes |observer| as the observer for next predicted navigation.
  void RemoveObserver(Observer* observer);

  // Notifies |this| that the visibility of web contents tracked by |client| has
  // changed or if user starts a new navigation corresponding to |web_contents|.
  // Might be called more than once with the same |is_in_foreground| and
  // |web_contents| in case user starts a new navigation with same
  // |web_contents|.
  void OnWebContentsVisibilityChanged(content::WebContents* web_contents,
                                      bool is_in_foreground);

  // Notifies |this| that the web contents tracked by |client| has destroyed.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // Overrides the tick clock used by |this| for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Returns true if the browser app is likely to be in foreground and being
  // interacted by the user. This is heuristically computed by observing loading
  // and visibility of web contents.
  bool IsBrowserAppLikelyInForeground() const;

 private:
  // List of observers are currently registered to receive notifications for the
  // next predicted navigations.
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Last known prediction.
  std::optional<Prediction> last_prediction_;

  // Manages preconnecting to the user's default search engine.
  SearchEnginePreconnector search_engine_preconnector_;

  std::unordered_set<raw_ptr<content::WebContents, CtnExperimental>>
      visible_web_contents_;

  base::TimeTicks last_web_contents_state_change_time_;

  raw_ptr<const base::TickClock> tick_clock_;
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
