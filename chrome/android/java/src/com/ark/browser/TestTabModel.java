package com.ark.browser;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.List;

public class TestTabModel implements TabModel {
    @Override
    public boolean isIncognito() {
        return false;
    }

    @Override
    public int index() {
        return 0;
    }

    @Override
    public int getCount() {
        return 0;
    }

    @Override
    public Tab getTabAt(int index) {
        return null;
    }

    @Override
    public int indexOf(Tab tab) {
        return 0;
    }

    @Override
    public Profile getProfile() {
        return null;
    }

    @Override
    public boolean closeTab(Tab tab) {
        return false;
    }

    @Override
    public boolean closeTab(Tab tab, boolean animate, boolean uponExit, boolean canUndo) {
        return false;
    }

    @Override
    public boolean closeTab(Tab tab, @Nullable Tab recommendedNextTab, boolean animate, boolean uponExit, boolean canUndo) {
        return false;
    }

    @Override
    public Tab getNextTabIfClosed(int id, boolean uponExit) {
        return null;
    }

    @Override
    public void closeMultipleTabs(List<Tab> tabs, boolean canUndo) {

    }

    @Override
    public void closeAllTabs() {

    }

    @Override
    public void closeAllTabs(boolean uponExit) {

    }

    @Override
    public boolean supportsPendingClosures() {
        return false;
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return false;
    }

    @Override
    public void commitAllTabClosures() {

    }

    @Override
    public void commitTabClosure(int tabId) {

    }

    @Override
    public void cancelTabClosure(int tabId) {

    }

    @Override
    public void openMostRecentlyClosedEntry() {

    }

    @Override
    public TabList getComprehensiveModel() {
        return null;
    }

    @Override
    public void setIndex(int i, int type, boolean skipLoadingTab) {

    }

    @Override
    public boolean isActiveModel() {
        return false;
    }

    @Override
    public void moveTab(int id, int newIndex) {

    }

    @Override
    public void destroy() {

    }

    @Override
    public void addTab(Tab tab, int index, int type, int creationState) {

    }

    @Override
    public void removeTab(Tab tab) {

    }

    @Override
    public void addObserver(TabModelObserver observer) {

    }

    @Override
    public void removeObserver(TabModelObserver observer) {

    }

    @Override
    public void setActive(boolean active) {

    }
}
