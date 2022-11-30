// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DATATYPE_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DATATYPE_HELPER_H_

class SyncTest;

namespace sync_datatype_helper {

// Associates an instance of SyncTest with sync_datatype_helper. Must be
// called before any of the methods in the per-datatype helper namespaces can be
// used.
void AssociateWithTest(SyncTest* test);

// Returns a pointer to the instance of SyncTest associated with the
// per-datatype helpers after making sure it is valid.
SyncTest* test();

}  // namespace sync_datatype_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DATATYPE_HELPER_H_
