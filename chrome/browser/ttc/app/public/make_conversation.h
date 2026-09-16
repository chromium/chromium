// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
#define CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_

#include <memory>

class Profile;

namespace ttc {

class Conversation;

// Creates the production Conversation implementation. This is the only part of
// //chrome/browser/ttc/app visible to code outside of it.
std::unique_ptr<Conversation> MakeConversationImpl(Profile* profile);

}  // namespace ttc

#endif  // CHROME_BROWSER_TTC_APP_PUBLIC_MAKE_CONVERSATION_H_
