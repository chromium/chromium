// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/store.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {DefaultUserImage, UserImage, UserInfo} from '../../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change user state.
 */

export enum UserActionName {
  SET_DEFAULT_USER_IMAGES = 'set_default_user_images',
  SET_PROFILE_IMAGE = 'set_profile_image',
  SET_IS_CAMERA_PRESENT = 'set_is_camera_present',
  SET_USER_IMAGE = 'set_user_image',
  SET_USER_INFO = 'set_user_info',
  SET_USER_IMAGE_IS_ENTERPRISE_MANAGED = 'set_user_image_is_enterprise_managed',
}

export type UserActions = SetIsCameraPresentAction|SetUserImageAction|
    SetDefaultUserImagesAction|SetUserInfoAction|SetProfileImageAction|
    SetUserImageIsEnterpriseManagedAction;

export interface SetIsCameraPresentAction extends Action {
  name: UserActionName.SET_IS_CAMERA_PRESENT;
  isCameraPresent: boolean;
}


export function setIsCameraPresentAction(isCameraPresent: boolean):
    SetIsCameraPresentAction {
  return {
    name: UserActionName.SET_IS_CAMERA_PRESENT,
    isCameraPresent,
  };
}

export interface SetUserImageAction extends Action {
  name: UserActionName.SET_USER_IMAGE;
  image: UserImage;
}


export function setUserImageAction(image: UserImage): SetUserImageAction {
  return {name: UserActionName.SET_USER_IMAGE, image};
}

export interface SetDefaultUserImagesAction extends Action {
  name: UserActionName.SET_DEFAULT_USER_IMAGES;
  defaultUserImages: DefaultUserImage[];
}


export function setDefaultUserImagesAction(
    defaultUserImages: DefaultUserImage[]): SetDefaultUserImagesAction {
  return {
    name: UserActionName.SET_DEFAULT_USER_IMAGES,
    defaultUserImages,
  };
}

export interface SetUserInfoAction extends Action {
  name: UserActionName.SET_USER_INFO;
  user_info: UserInfo;
}


/**
 * Notify that the app has finished loading user info. Will be called with null
 * on error.
 */
export function setUserInfoAction(userInfo: UserInfo): SetUserInfoAction {
  return {
    name: UserActionName.SET_USER_INFO,
    user_info: userInfo,
  };
}

export interface SetProfileImageAction extends Action {
  name: UserActionName.SET_PROFILE_IMAGE;
  profileImage: Url;
}


export function setProfileImageAction(profileImage: Url):
    SetProfileImageAction {
  return {
    name: UserActionName.SET_PROFILE_IMAGE,
    profileImage,
  };
}

export interface SetUserImageIsEnterpriseManagedAction extends Action {
  name: UserActionName.SET_USER_IMAGE_IS_ENTERPRISE_MANAGED;
  isEnterpriseManaged: boolean;
}


export function setUserImageIsEnterpriseManagedAction(
    isEnterpriseManaged: boolean): SetUserImageIsEnterpriseManagedAction {
  return {
    name: UserActionName.SET_USER_IMAGE_IS_ENTERPRISE_MANAGED,
    isEnterpriseManaged,
  };
}
