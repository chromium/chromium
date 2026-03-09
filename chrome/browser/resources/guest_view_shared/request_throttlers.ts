// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Value} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

export interface Header {
  name: string;
  value: string;
}

export class OnBeforeSendHeadersParams {
  constructor(
      // Types get validated when creating the SlimWebViewGuest.
      public types: string[],
      public includeSubFrameRequests: boolean,
      public addHeaders: Header[],
  ) {}

  toValue(): Value {
    return {
      dictionaryValue: {
        storage: {
          resourceTypes: {
            listValue: {
              storage: this.types.map((type) => {
                return {
                  stringValue: type,
                };
              }),
            },
          },
          includeSubFrameRequests: {
            boolValue: this.includeSubFrameRequests,
          },
          addHeaders: {
            listValue: {
              storage: this.addHeaders.map((header) => {
                return {
                  dictionaryValue: {
                    storage: {
                      name: {stringValue: header.name},
                      value: {stringValue: header.value},
                    },
                  },
                };
              }),
            },
          },
        },
      },
    };
  }
}
