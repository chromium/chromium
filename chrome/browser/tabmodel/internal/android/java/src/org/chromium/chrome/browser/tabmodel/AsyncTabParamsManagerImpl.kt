// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.tabmodel

import android.util.SparseArray

import org.chromium.chrome.browser.tab.Tab

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

  override fun getAsyncTabParams() = mAsyncTabParams

  override fun hasParamsForTabId(tabId: Int) = mAsyncTabParams[tabId] != null

  override fun hasParamsWithTabToReparent(): Boolean {
    forEachTab { return true }
    return false
  }

  override fun remove(tabId: Int): AsyncTabParams? {
    val data = mAsyncTabParams[tabId]
    mAsyncTabParams.remove(tabId)
    return data
  }

  private inline fun forEachTab(action: (tab: Tab) -> Unit) {
    val params = mAsyncTabParams
    for (i in 0 until params.size()) {
      val tab = params.valueAt(i).tabToReparent
      if (tab != null) {
        action(tab)
      }
    }
  }

  private class AsyncTabsIncognitoTabHost(
    private val mAsyncTabParamsManager: AsyncTabParamsManagerImpl
  ) : IncognitoTabHost {

    override fun hasIncognitoTabs(): Boolean {
      mAsyncTabParamsManager.forEachTab {
        if (it.isIncognitoBranded) return true
      }
      return false
    }

    override fun closeAllIncognitoTabs() {
      val params = mAsyncTabParamsManager.mAsyncTabParams
      // removeAt() does not invalidate indices so long as no read operations are made.
      val clone = params.clone()
      for (i in 0 until clone.size()) {
        if (clone.valueAt(i).tabToReparent?.isIncognitoBranded ?: false) {
          params.removeAt(i)
        }
      }
    }

    override fun isActiveModel() = false
  }
}
