// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class helps to remember what domains may be needed to be resolved when a
// navigation takes place to a given URL.  This information is gathered when a
// navigation to a subresource identifies a referring URL.
// When future navigations take place to known referrer sites, then we
// speculatively either pre-warm a TCP/IP conneciton, or at a minimum, resolve
// the host name via DNS.

// All access to this class is performed via the Predictor class, which only
// operates on the IO thread.

#ifndef CHROME_BROWSER_NET_REFERRER_H_
#define CHROME_BROWSER_NET_REFERRER_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "net/base/host_port_pair.h"
#include "url/gurl.h"

namespace chrome_browser_net {

//------------------------------------------------------------------------------
// For each hostname in a Referrer, we have a ReferrerValue.  It indicates
// exactly how much value (re: latency reduction, or connection use) has
// resulted from having this entry.
class ReferrerValue {
 public:
  ReferrerValue();

  // Used during deserialization.
  void SetSubresourceUseRate(double rate) { subresource_use_rate_ = rate; }

  base::Time birth_time() const { return birth_time_; }

  // Record the fact that we navigated to the associated subresource URL.  This
  // will increase the value of the expected subresource_use_rate_
  void SubresourceIsNeeded();

  // Record the fact that the referrer of this subresource was observed. This
  // will diminish the expected subresource_use_rate_ (and will only be
  // counteracted later if we really needed this subresource as a consequence
  // of our associated referrer.)
  void ReferrerWasObserved();

  int64_t navigation_count() const { return navigation_count_; }
  double subresource_use_rate() const { return subresource_use_rate_; }

  int64_t preconnection_count() const { return preconnection_count_; }
  void IncrementPreconnectionCount() { ++preconnection_count_; }

  int64_t preresolution_count() const { return preresolution_count_; }
  void preresolution_increment() { ++preresolution_count_; }

 private:
  const base::Time birth_time_;

  // The number of times this item was navigated to with the fixed referrer.
  int64_t navigation_count_;

  // The number of times this item was preconnected as a consequence of its
  // referrer.
  int64_t preconnection_count_;

  // The number of times this item was pre-resolved (via DNS) as a consequence
  // of its referrer.
  int64_t preresolution_count_;

  // A smoothed estimate of the expected number of connections that will be made
  // to this subresource.
  double subresource_use_rate_;
};

//------------------------------------------------------------------------------
// A list of domain names to pre-resolve. The names are the keys to this map,
// and the values indicate the amount of benefit derived from having each name
// around.
typedef std::map<GURL, ReferrerValue> SubresourceMap;

//------------------------------------------------------------------------------
// There is one Referrer instance for each hostname that has acted as an HTTP
// referer (note mispelling is intentional) for a hostname that was otherwise
// unexpectedly navgated towards ("unexpected" in the sense that the hostname
// was probably needed as a subresource of a page, and was not otherwise
// predictable until the content with the reference arrived).  Most typically,
// an outer page was a page fetched by the user, and this instance lists names
// in SubresourceMap which are subresources and that were needed to complete the
// rendering of the outer page.
class Referrer : public SubresourceMap {
 public:
  Referrer();
  void IncrementUseCount() { ++use_count_; }
  int64_t use_count() const { return use_count_; }

  // Add the indicated url to the list that are resolved via DNS when the user
  // navigates to this referrer.  Note that if the list is long, an entry may be
  // discarded to make room for this insertion.
  void SuggestHost(const GURL& url);

  // Provide methods for persisting, and restoring contents into a Value class.
  std::unique_ptr<base::Value::List> Serialize() const;
  void Deserialize(const base::Value& referrers);

 private:
  // Helper function for pruning list.  Metric for usefulness is "large accrued
  // value," in the form of latency_ savings associated with a host name.  We
  // also give credit for a name being newly added, by scalling latency per
  // lifetime (time since birth).  For instance, when two names have accrued
  // the same latency_ savings, the older one is less valuable as it didn't
  // accrue savings as quickly.
  void DeleteLeastUseful();

  // The number of times this referer had its subresources scanned for possible
  // preconnection or DNS preresolution.
  int64_t use_count_;

  // We put these into a std::map<>, so we need copy constructors.
  // DISALLOW_COPY_AND_ASSIGN(Referrer);
  // TODO(jar): Consider optimization to use pointers to these instances, and
  // avoid deep copies during re-alloc of the containing map.
};

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_REFERRER_H_
