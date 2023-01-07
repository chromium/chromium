// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ChangedPackages;
import android.content.pm.FeatureInfo;
import android.content.pm.InstrumentationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.content.pm.PermissionGroupInfo;
import android.content.pm.PermissionInfo;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.SharedLibraryInfo;
import android.content.pm.VersionedPackage;
import android.content.res.Resources;
import android.content.res.XmlResourceParser;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.UserHandle;

import java.util.List;

/**
 * Allows instrumentation tests to wrap the real PackageManager and override calls they wish to
 * control the return values of (eg. by returning a wrapped real PackageManager from the Application
 * Context).
 */
public class PackageManagerWrapper extends PackageManager {
    private PackageManager mWrapped;

    public PackageManagerWrapper(PackageManager wrapped) {
        super();
        mWrapped = wrapped;
    }

    @Override
    @Deprecated
    public void addPackageToPreferred(String packageName) {
        mWrapped.addPackageToPreferred(packageName);
    }

    @Override
    public boolean addPermission(PermissionInfo info) {
        return mWrapped.addPermission(info);
    }

    @Override
    public boolean addPermissionAsync(PermissionInfo info) {
        return mWrapped.addPermissionAsync(info);
    }

    @Override
    @Deprecated
    public void addPreferredActivity(
            IntentFilter filter, int match, ComponentName[] set, ComponentName activity) {
        mWrapped.addPreferredActivity(filter, match, set, activity);
    }

    @Override
    public String[] canonicalToCurrentPackageNames(String[] names) {
        return mWrapped.canonicalToCurrentPackageNames(names);
    }

    @Override
    public int checkPermission(String permName, String pkgName) {
        return mWrapped.checkPermission(permName, pkgName);
    }

    @Override
    public int checkSignatures(String pkg1, String pkg2) {
        return mWrapped.checkSignatures(pkg1, pkg2);
    }

    @Override
    public int checkSignatures(int uid1, int uid2) {
        return mWrapped.checkSignatures(uid1, uid2);
    }

    @Override
    public void clearInstantAppCookie() {
        mWrapped.clearInstantAppCookie();
    }

    @Override
    public String[] currentToCanonicalPackageNames(String[] names) {
        return mWrapped.currentToCanonicalPackageNames(names);
    }

    @Override
    public ActivityInfo getActivityInfo(ComponentName component, int flags)
            throws NameNotFoundException {
        return mWrapped.getActivityInfo(component, flags);
    }

    @Override
    public List<PermissionGroupInfo> getAllPermissionGroups(int flags) {
        return mWrapped.getAllPermissionGroups(flags);
    }

    @Override
    public ApplicationInfo getApplicationInfo(String packageName, int flags)
            throws NameNotFoundException {
        return mWrapped.getApplicationInfo(packageName, flags);
    }

    @Override
    public List<PackageInfo> getInstalledPackages(int flags) {
        return mWrapped.getInstalledPackages(flags);
    }

    @Override
    public Intent getLaunchIntentForPackage(String packageName) {
        return mWrapped.getLaunchIntentForPackage(packageName);
    }

    @Override
    public Intent getLeanbackLaunchIntentForPackage(String packageName) {
        return mWrapped.getLeanbackLaunchIntentForPackage(packageName);
    }

    @Override
    public int[] getPackageGids(String packageName) throws NameNotFoundException {
        return mWrapped.getPackageGids(packageName);
    }

    @Override
    public int[] getPackageGids(String packageName, int flags) throws NameNotFoundException {
        return mWrapped.getPackageGids(packageName, flags);
    }

    @Override
    public PackageInfo getPackageInfo(String packageName, int flags) throws NameNotFoundException {
        return mWrapped.getPackageInfo(packageName, flags);
    }

    @Override
    public int getPackageUid(String packageName, int flags) throws NameNotFoundException {
        return mWrapped.getPackageUid(packageName, flags);
    }

    @Override
    public List<PackageInfo> getPackagesHoldingPermissions(String[] permissions, int flags) {
        return mWrapped.getPackagesHoldingPermissions(permissions, flags);
    }

    @Override
    public PermissionGroupInfo getPermissionGroupInfo(String name, int flags)
            throws NameNotFoundException {
        return mWrapped.getPermissionGroupInfo(name, flags);
    }

    @Override
    public PermissionInfo getPermissionInfo(String name, int flags) throws NameNotFoundException {
        return mWrapped.getPermissionInfo(name, flags);
    }

    @Override
    public ProviderInfo getProviderInfo(ComponentName component, int flags)
            throws NameNotFoundException {
        return mWrapped.getProviderInfo(component, flags);
    }

    @Override
    public ActivityInfo getReceiverInfo(ComponentName component, int flags)
            throws NameNotFoundException {
        return mWrapped.getReceiverInfo(component, flags);
    }

    @Override
    public ServiceInfo getServiceInfo(ComponentName component, int flags)
            throws NameNotFoundException {
        return mWrapped.getServiceInfo(component, flags);
    }

    @Override
    public boolean isPermissionRevokedByPolicy(String permName, String pkgName) {
        return mWrapped.isPermissionRevokedByPolicy(permName, pkgName);
    }

    @Override
    public List<PermissionInfo> queryPermissionsByGroup(String group, int flags)
            throws NameNotFoundException {
        return mWrapped.queryPermissionsByGroup(group, flags);
    }

    @Override
    public void removePermission(String name) {
        mWrapped.removePermission(name);
    }

    @Override
    public String[] getPackagesForUid(int uid) {
        return mWrapped.getPackagesForUid(uid);
    }

    @Override
    public String getNameForUid(int uid) {
        return mWrapped.getNameForUid(uid);
    }

    @Override
    public List<ApplicationInfo> getInstalledApplications(int flags) {
        return mWrapped.getInstalledApplications(flags);
    }

    @Override
    public String[] getSystemSharedLibraryNames() {
        return mWrapped.getSystemSharedLibraryNames();
    }

    @Override
    public FeatureInfo[] getSystemAvailableFeatures() {
        return mWrapped.getSystemAvailableFeatures();
    }

    @Override
    public boolean hasSystemFeature(String name) {
        return mWrapped.hasSystemFeature(name);
    }

    @Override
    public boolean hasSystemFeature(String name, int version) {
        return mWrapped.hasSystemFeature(name, version);
    }

    @Override
    public ResolveInfo resolveActivity(Intent intent, int flags) {
        return mWrapped.resolveActivity(intent, flags);
    }

    @Override
    public List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
        return mWrapped.queryIntentActivities(intent, flags);
    }

    @Override
    public List<ResolveInfo> queryIntentActivityOptions(
            ComponentName caller, Intent[] specifics, Intent intent, int flags) {
        return mWrapped.queryIntentActivityOptions(caller, specifics, intent, flags);
    }

    @Override
    public List<ResolveInfo> queryBroadcastReceivers(Intent intent, int flags) {
        return mWrapped.queryBroadcastReceivers(intent, flags);
    }

    @Override
    public ResolveInfo resolveService(Intent intent, int flags) {
        return mWrapped.resolveService(intent, flags);
    }

    @Override
    public List<ResolveInfo> queryIntentServices(Intent intent, int flags) {
        return mWrapped.queryIntentServices(intent, flags);
    }

    @Override
    public List<ResolveInfo> queryIntentContentProviders(Intent intent, int flags) {
        return mWrapped.queryIntentContentProviders(intent, flags);
    }

    @Override
    public ProviderInfo resolveContentProvider(String name, int flags) {
        return mWrapped.resolveContentProvider(name, flags);
    }

    @Override
    public List<ProviderInfo> queryContentProviders(String processName, int uid, int flags) {
        return mWrapped.queryContentProviders(processName, uid, flags);
    }

    @Override
    public InstrumentationInfo getInstrumentationInfo(ComponentName className, int flags)
            throws NameNotFoundException {
        return mWrapped.getInstrumentationInfo(className, flags);
    }

    @Override
    public List<InstrumentationInfo> queryInstrumentation(String targetPackage, int flags) {
        return mWrapped.queryInstrumentation(targetPackage, flags);
    }

    @Override
    public Drawable getDrawable(String packageName, int resid, ApplicationInfo appInfo) {
        return mWrapped.getDrawable(packageName, resid, appInfo);
    }

    @Override
    public Drawable getActivityIcon(ComponentName activityName) throws NameNotFoundException {
        return mWrapped.getActivityIcon(activityName);
    }

    @Override
    public Drawable getActivityIcon(Intent intent) throws NameNotFoundException {
        return mWrapped.getActivityIcon(intent);
    }

    @Override
    public Drawable getActivityBanner(ComponentName activityName) throws NameNotFoundException {
        return mWrapped.getActivityBanner(activityName);
    }

    @Override
    public Drawable getActivityBanner(Intent intent) throws NameNotFoundException {
        return mWrapped.getActivityBanner(intent);
    }

    @Override
    public Drawable getDefaultActivityIcon() {
        return mWrapped.getDefaultActivityIcon();
    }

    @Override
    public Drawable getApplicationIcon(ApplicationInfo info) {
        return mWrapped.getApplicationIcon(info);
    }

    @Override
    public Drawable getApplicationIcon(String packageName) throws NameNotFoundException {
        return mWrapped.getApplicationIcon(packageName);
    }

    @Override
    public Drawable getApplicationBanner(ApplicationInfo info) {
        return mWrapped.getApplicationBanner(info);
    }

    @Override
    public Drawable getApplicationBanner(String packageName) throws NameNotFoundException {
        return mWrapped.getApplicationBanner(packageName);
    }

    @Override
    public Drawable getActivityLogo(ComponentName activityName) throws NameNotFoundException {
        return mWrapped.getActivityLogo(activityName);
    }

    @Override
    public Drawable getActivityLogo(Intent intent) throws NameNotFoundException {
        return mWrapped.getActivityLogo(intent);
    }

    @Override
    public Drawable getApplicationLogo(ApplicationInfo info) {
        return mWrapped.getApplicationLogo(info);
    }

    @Override
    public Drawable getApplicationLogo(String packageName) throws NameNotFoundException {
        return mWrapped.getApplicationLogo(packageName);
    }

    @Override
    public Drawable getUserBadgedIcon(Drawable icon, UserHandle user) {
        return mWrapped.getUserBadgedIcon(icon, user);
    }

    @Override
    public Drawable getUserBadgedDrawableForDensity(
            Drawable drawable, UserHandle user, Rect badgeLocation, int badgeDensity) {
        return mWrapped.getUserBadgedDrawableForDensity(
                drawable, user, badgeLocation, badgeDensity);
    }

    @Override
    public CharSequence getUserBadgedLabel(CharSequence label, UserHandle user) {
        return mWrapped.getUserBadgedLabel(label, user);
    }

    @Override
    public CharSequence getText(String packageName, int resid, ApplicationInfo appInfo) {
        return mWrapped.getText(packageName, resid, appInfo);
    }

    @Override
    public XmlResourceParser getXml(String packageName, int resid, ApplicationInfo appInfo) {
        return mWrapped.getXml(packageName, resid, appInfo);
    }

    @Override
    public CharSequence getApplicationLabel(ApplicationInfo info) {
        return mWrapped.getApplicationLabel(info);
    }

    @Override
    public Resources getResourcesForActivity(ComponentName activityName)
            throws NameNotFoundException {
        return mWrapped.getResourcesForActivity(activityName);
    }

    @Override
    public Resources getResourcesForApplication(ApplicationInfo app) throws NameNotFoundException {
        return mWrapped.getResourcesForApplication(app);
    }

    @Override
    public Resources getResourcesForApplication(String appPackageName)
            throws NameNotFoundException {
        return mWrapped.getResourcesForApplication(appPackageName);
    }

    @Override
    public void verifyPendingInstall(int id, int verificationCode) {
        mWrapped.verifyPendingInstall(id, verificationCode);
    }

    @Override
    public void extendVerificationTimeout(
            int id, int verificationCodeAtTimeout, long millisecondsToDelay) {
        mWrapped.extendVerificationTimeout(id, verificationCodeAtTimeout, millisecondsToDelay);
    }

    @Override
    public void setInstallerPackageName(String targetPackage, String installerPackageName) {
        mWrapped.setInstallerPackageName(targetPackage, installerPackageName);
    }

    @Override
    public String getInstallerPackageName(String packageName) {
        return mWrapped.getInstallerPackageName(packageName);
    }

    @Deprecated
    @Override
    public void removePackageFromPreferred(String packageName) {
        mWrapped.removePackageFromPreferred(packageName);
    }

    @Override
    public List<PackageInfo> getPreferredPackages(int flags) {
        return mWrapped.getPreferredPackages(flags);
    }

    @Override
    public void clearPackagePreferredActivities(String packageName) {
        mWrapped.clearPackagePreferredActivities(packageName);
    }

    @Override
    public int getPreferredActivities(
            List<IntentFilter> outFilters, List<ComponentName> outActivities, String packageName) {
        return mWrapped.getPreferredActivities(outFilters, outActivities, packageName);
    }

    @Override
    public void setComponentEnabledSetting(ComponentName componentName, int newState, int flags) {
        mWrapped.setComponentEnabledSetting(componentName, newState, flags);
    }

    @Override
    public int getComponentEnabledSetting(ComponentName componentName) {
        return mWrapped.getComponentEnabledSetting(componentName);
    }

    @Override
    public void setApplicationEnabledSetting(String packageName, int newState, int flags) {
        mWrapped.setApplicationEnabledSetting(packageName, newState, flags);
    }

    @Override
    public int getApplicationEnabledSetting(String packageName) {
        return mWrapped.getApplicationEnabledSetting(packageName);
    }

    @Override
    public boolean isSafeMode() {
        return mWrapped.isSafeMode();
    }

    @Override
    public PackageInstaller getPackageInstaller() {
        return mWrapped.getPackageInstaller();
    }

    // O Developer Preview

    @Override
    public boolean canRequestPackageInstalls() {
        return mWrapped.canRequestPackageInstalls();
    }

    @Override
    public ChangedPackages getChangedPackages(int sequenceNumber) {
        return mWrapped.getChangedPackages(sequenceNumber);
    }

    @Override
    public byte[] getInstantAppCookie() {
        return mWrapped.getInstantAppCookie();
    }

    @Override
    public int getInstantAppCookieMaxBytes() {
        return mWrapped.getInstantAppCookieMaxBytes();
    }

    @Override
    public PackageInfo getPackageInfo(VersionedPackage versionedPackage, int flags)
            throws PackageManager.NameNotFoundException {
        return mWrapped.getPackageInfo(versionedPackage, flags);
    }

    @Override
    public List<SharedLibraryInfo> getSharedLibraries(int flags) {
        return mWrapped.getSharedLibraries(flags);
    }

    @Override
    public boolean isInstantApp() {
        return mWrapped.isInstantApp();
    }

    @Override
    public boolean isInstantApp(String packageName) {
        return mWrapped.isInstantApp(packageName);
    }

    @Override
    public void setApplicationCategoryHint(String packageName, int categoryHint) {
        mWrapped.setApplicationCategoryHint(packageName, categoryHint);
    }

    @Override
    public void updateInstantAppCookie(byte[] cookie) {
        mWrapped.updateInstantAppCookie(cookie);
    }

    // This is a hidden abstract method in the base class so we can't mark it as
    // overridden, or call the real implementation without reflection. Nothing
    // currently seems to actually care about the result of this function under
    // testing, so we can just return null for now.
    protected Drawable getUserBadgeForDensity(UserHandle userHandle, int i) {
        return null;
    }
}
