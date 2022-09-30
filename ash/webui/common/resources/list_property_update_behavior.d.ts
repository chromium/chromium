// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ListPropertyUpdateBehavior {
  updateList(
      propertyPath: string, identityGetter: ((arg0: any) => (any | string)),
      updatedList: any[], identityBasedUpdate?: boolean): boolean;
}

declare const ListPropertyUpdateBehavior: object;
