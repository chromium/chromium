// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShoppingListApiProxy, ShoppingListApiProxyImpl} from '//shopping-insights-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';

function getProxy(): ShoppingListApiProxy {
  return ShoppingListApiProxyImpl.getInstance();
}

function initialize() {
  getProxy().getProductInfoForCurrentUrl().then(res => {
    const title = res.productInfo.title;
    if (title) {
      document.getElementById('productTitle')!.innerText = title;
    }
  });
}

document.addEventListener('DOMContentLoaded', initialize);
