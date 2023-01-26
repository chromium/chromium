// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tabmodel

import android.util.SparseArray

/**
 * Data that will be used later when a tab is opened via an intent. Often only the necessary subset
 * of the data will be set. All data is removed once the tab finishes initializing.
 */
class AsyncTabParamsManagerImpl internal constructor() : AsyncTabParamsManager {
  /** A map of tab IDs to AsyncTabParams consumed by Activities started asynchronously. */
  private val mAsyncTabParams = SparseArray<AsyncTabParams>()
  private var mAddedToIncognitoTabHostRegistry = false

  override fun add(tabId: Int, params: AsyncTabParams) {
    mAsyncTabParams.put(tabId, params)
    if (!mAddedToIncognitoTabHostRegistry) {
      // Make sure async incognito tabs are taken into account when, for example,
      // checking if any incognito tabs exist.
      IncognitoTabHostRegistry.getInstance().register(AsyncTabsIncognitoTabHost(this))
      mAddedToIncognitoTabHostRegistry = true
    }
  }

  override fun hasParamsForTabId(tabId: Int) = mAsyncTabParams[tabId] != null

  override fun hasParamsWithTabToReparent(): Boolean {
    for (i in 0 until mAsyncTabParams.size()) {
      if (mAsyncTabParams[mAsyncTabParams.keyAt(i)].tabToReparent == null) continue
      return true
    }
    return false
  }

  override fun getAsyncTabParams(): SparseArray<AsyncTabParams> {
    return mAsyncTabParams
  }

  override fun remove(tabId: Int): AsyncTabParams? {
    val data = mAsyncTabParams[tabId]
    mAsyncTabParams.remove(tabId)
    return data
  }

  private class AsyncTabsIncognitoTabHost(
    private val mAsyncTabParamsManager: AsyncTabParamsManager
  ) : IncognitoTabHost {
    override fun hasIncognitoTabs(): Boolean {
      val asyncTabParams = mAsyncTabParamsManager.asyncTabParams
      for (i in 0 until asyncTabParams.size()) {
        val tab = asyncTabParams.valueAt(i).tabToReparent
        if (tab != null && tab.isIncognito) {
          return true
        }
      }
      return false
    }

    override fun closeAllIncognitoTabs() {
      val asyncTabParams = mAsyncTabParamsManager.asyncTabParams
      for (i in 0 until asyncTabParams.size()) {
        val tab = asyncTabParams.valueAt(i).tabToReparent
        if (tab != null && tab.isIncognito) {
          mAsyncTabParamsManager.remove(tab.id)
        }
      }
    }

    override fun isActiveModel(): Boolean {
      return false
    }
  }
}
