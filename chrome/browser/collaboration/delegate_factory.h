// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COLLABORATION_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_COLLABORATION_DELEGATE_FACTORY_H_

namespace collaboration {

// A factory to create a CollaborationControllerDelegate.
class DelegateFactory {
  // Create an unique CollaborationControllerDelegate for the given
  // CollaborationController.
  static unique_ptr<CollaborationControllerDelegate> BuildDelegateForController(
      const CollaborationDelegateArgs& args,
      CollaborationController* controller);
}

}  // namespace collaboration

#endif  // CHROME_BROWSER_COLLABORATION_DELEGATE_FACTORY_H_
