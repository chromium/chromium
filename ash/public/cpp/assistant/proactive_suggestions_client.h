// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
#define ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace viz {
enum class VerticalScrollDirection;
}  // namespace viz

namespace ash {

class ProactiveSuggestions;

// A browser client which observes changes to the singleton BrowserList on
// behalf of Assistant to provide it with information necessary to retrieve
// proactive content suggestions.
class ASH_PUBLIC_EXPORT ProactiveSuggestionsClient {
 public:
  // A delegate interface for the ProactiveSuggestionsClient.
  class Delegate {
   public:
    // Invoked when the proactive suggestions client is being destroyed so as to
    // give the delegate an opportunity to remove itself.
    virtual void OnProactiveSuggestionsClientDestroying() {}

    // Invoked when the |proactive_suggestions| associated with the browser have
    // changed. Note that |proactive_suggestions| may be |nullptr| if none
    // exist.
    virtual void OnProactiveSuggestionsChanged(
        scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {}

    // Invoked when the vertical |scroll_direction| is changed in the source
    // web contents associated with the active set of proactive suggestions.
    virtual void OnSourceVerticalScrollDirectionChanged(
        viz::VerticalScrollDirection scroll_direction) {}

   protected:
    Delegate() = default;
    virtual ~Delegate() = default;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Returns the singleton instance.
  static ProactiveSuggestionsClient* Get();

  // Sets the delegate for the client which may be |nullptr|.
  virtual void SetDelegate(Delegate* delegate) = 0;

 protected:
  ProactiveSuggestionsClient();
  virtual ~ProactiveSuggestionsClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(ProactiveSuggestionsClient);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_PROACTIVE_SUGGESTIONS_CLIENT_H_
