// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * import {Resolution} from '../type.js';
 */
var Resolution = Resolution || {};

/* eslint-disable no-unused-vars */

/**
 * Object of device id, preferred capture resolution and all
 * available resolutions for a particular video device.
 * @typedef {{prefResol: !Resolution, resols: !ResolutionList}}
 */
cca.views.ResolutionConfig;

/**
 * Photo and video resolution configuration for a particular video device.
 * @typedef {{
 *   deviceId: string,
 *   photo: !cca.views.ResolutionConfig,
 *   video: !cca.views.ResolutionConfig,
 * }}
 */
cca.views.DeviceSetting;

/* eslint-enable no-unused-vars */

/**
 * Creates the base controller of settings view.
 * @param {string} selector Selector text of the view's root element.
 * @param {!Object<string, !function(Event=)>=} itemHandlers Click-handlers
 *     mapped by element ids.
 * @extends {cca.views.View}
 * @constructor
 */
cca.views.BaseSettings = function(selector, itemHandlers = {}) {
  cca.views.View.call(this, selector, true, true);

  this.root.querySelector('.menu-header button')
      .addEventListener('click', () => this.leave());
  this.root.querySelectorAll('.menu-item').forEach((element) => {
    /** @type {!function(Event=)|undefined} */
    const handler = itemHandlers[element.id];
    if (handler) {
      element.addEventListener('click', handler);
    }
  });
};

cca.views.BaseSettings.prototype = {
  __proto__: cca.views.View.prototype,
};

/**
 * @override
 */
cca.views.BaseSettings.prototype.focus = function() {
  this.rootElement_.querySelector('[tabindex]').focus();
};

/**
 * Opens sub-settings.
 * @param {string} id Settings identifier.
 * @private
 */
cca.views.BaseSettings.prototype.openSubSettings = function(id) {
  // Dismiss master-settings if sub-settings was dimissed by background click.
  cca.nav.open(id).then((cond) => cond && cond.bkgnd && this.leave());
};

/**
 * Creates the controller of master settings view.
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.MasterSettings = function() {
  cca.views.BaseSettings.call(this, '#settings', {
    'settings-gridtype': () => this.openSubSettings('gridsettings'),
    'settings-timerdur': () => this.openSubSettings('timersettings'),
    'settings-resolution': () => this.openSubSettings('resolutionsettings'),
    'settings-expert': () => this.openSubSettings('expertsettings'),
    'settings-feedback': () => this.openFeedback(),
    'settings-help': () => cca.util.openHelp(),
  });

  // End of properties, seal the object.
  Object.seal(this);

  document.querySelector('#settings-feedback').hidden =
      !cca.util.isChromeVersionAbove(72);  // Feedback available since M72.
};

cca.views.MasterSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Opens feedback.
 * @private
 */
cca.views.MasterSettings.prototype.openFeedback = function() {
  var data = {
    'categoryTag': 'chromeos-camera-app',
    'requestFeedback': true,
    'feedbackInfo': {
      'description': '',
      'systemInformation': [
        {key: 'APP ID', value: chrome.runtime.id},
        {key: 'APP VERSION', value: chrome.runtime.getManifest().version},
      ],
    },
  };
  const id = 'gfdkimpbcpahaombhbimeihdjnejgicl';  // Feedback extension id.
  chrome.runtime.sendMessage(id, data);
};

/**
 * Creates the controller of resolution settings view.
 * @param {!cca.device.DeviceInfoUpdater} infoUpdater
 * @param {!cca.device.PhotoConstraintsPreferrer} photoPreferrer
 * @param {!cca.device.VideoConstraintsPreferrer} videoPreferrer
 * @extends {cca.views.BaseSettings}
 * @constructor
 */
cca.views.ResolutionSettings = function(
    infoUpdater, photoPreferrer, videoPreferrer) {
  /**
   * @param {function(): ?cca.views.DeviceSetting} getSetting
   * @param {function(): !HTMLElement} getElement
   * @param {boolean} isPhoto
   * @return {!function()}
   */
  const createOpenMenuHandler = (getSetting, getElement, isPhoto) => () => {
    const setting = getSetting();
    if (setting === null) {
      console.error('Open settings of non-exist device.');
      return;
    }
    const element = getElement();
    if (element.classList.contains('multi-option')) {
      if (isPhoto) {
        this.openPhotoResSettings_(setting, element);
      } else {
        this.openVideoResSettings_(setting, element);
      }
    }
  };
  cca.views.BaseSettings.call(this, '#resolutionsettings', {
    'settings-front-photores': createOpenMenuHandler(
        () => this.frontSetting_, () => this.frontPhotoItem_, true),
    'settings-front-videores': createOpenMenuHandler(
        () => this.frontSetting_, () => this.frontVideoItem_, false),
    'settings-back-photores': createOpenMenuHandler(
        () => this.backSetting_, () => this.backPhotoItem_, true),
    'settings-back-videores': createOpenMenuHandler(
        () => this.backSetting_, () => this.backVideoItem_, false),
  });

  /**
   * @type {!cca.device.PhotoConstraintsPreferrer}
   * @private
   */
  this.photoPreferrer_ = photoPreferrer;

  /**
   * @type {!cca.device.VideoConstraintsPreferrer}
   * @private
   */
  this.videoPreferrer_ = videoPreferrer;

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.resMenu_ = /** @type {!HTMLElement} */ (
      document.querySelector('#resolutionsettings>div.menu'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.videoResMenu_ = /** @type {!HTMLElement} */ (
      document.querySelector('#videoresolutionsettings>div.menu'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.photoResMenu_ = /** @type {!HTMLElement} */ (
      document.querySelector('#photoresolutionsettings>div.menu'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.frontPhotoItem_ = /** @type {!HTMLElement} */ (
      document.querySelector('#settings-front-photores'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.frontVideoItem_ = /** @type {!HTMLElement} */ (
      document.querySelector('#settings-front-videores'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.backPhotoItem_ = /** @type {!HTMLElement} */ (
      document.querySelector('#settings-back-photores'));

  /**
   * @type {!HTMLElement}
   * @private
   */
  this.backVideoItem_ = /** @type {!HTMLElement} */ (
      document.querySelector('#settings-back-videores'));

  /**
   * @type {!HTMLTemplateElement}
   * @private
   */
  this.resItemTempl_ = /** @type {!HTMLTemplateElement} */ (
      document.querySelector('#resolution-item-template'));

  /**
   * @type {!HTMLTemplateElement}
   * @private
   */
  this.extcamItemTempl_ = /** @type {!HTMLTemplateElement} */ (
      document.querySelector('#extcam-resolution-item-template'));

  /**
   * Device setting of front camera. Null if no front camera.
   * @type {?cca.views.DeviceSetting}
   * @private
   */
  this.frontSetting_ = null;

  /**
   * Device setting of back camera. Null if no front camera.
   * @type {?cca.views.DeviceSetting}
   * @private
   */
  this.backSetting_ = null;

  /**
   * Device setting of external cameras.
   * @type {!Array<!cca.views.DeviceSetting>}
   * @private
   */
  this.externalSettings_ = [];

  /**
   * Device id of currently opened resolution setting view.
   * @type {?string}
   * @private
   */
  this.openedSettingDeviceId_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  infoUpdater.addDeviceChangeListener(async (updater) => {
    /** @type {?Array<!cca.device.Camera3DeviceInfo>} */
    const devices = await updater.getCamera3DevicesInfo();
    if (devices === null) {
      cca.state.set('no-resolution-settings', true);
      return;
    }

    this.frontSetting_ = this.backSetting_ = null;
    this.externalSettings_ = [];

    devices.forEach(({deviceId, facing, photoResols, videoResols}) => {
      const /** !cca.views.DeviceSetting */ deviceSetting = {
        deviceId,
        photo: {
          prefResol: /** @type {!Resolution} */ (
              photoPreferrer.getPrefResolution(deviceId)),
          resols:
              /* Filter out resolutions of megapixels < 0.1 i.e. megapixels
               * 0.0*/
              photoResols.filter((r) => r.area >= 100000),
        },
        video: {
          prefResol: /** @type {!Resolution} */ (
              videoPreferrer.getPrefResolution(deviceId)),
          resols: videoResols,
        },
      };
      switch (facing) {
        case cros.mojom.CameraFacing.CAMERA_FACING_FRONT:
          this.frontSetting_ = deviceSetting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_BACK:
          this.backSetting_ = deviceSetting;
          break;
        case cros.mojom.CameraFacing.CAMERA_FACING_EXTERNAL:
          this.externalSettings_.push(deviceSetting);
          break;
        default:
          console.error(`Ignore device of unknown facing: ${facing}`);
      }
    });
    this.updateResolutions_();
  });

  this.photoPreferrer_.setPreferredResolutionChangeListener(
      this.updateSelectedPhotoResolution_.bind(this));
  this.videoPreferrer_.setPreferredResolutionChangeListener(
      this.updateSelectedVideoResolution_.bind(this));
};

cca.views.ResolutionSettings.prototype = {
  __proto__: cca.views.BaseSettings.prototype,
};

/**
 * Template for generating option text from photo resolution width and height.
 * @param {!Resolution} r Resolution of text to be generated.
 * @param {!ResolutionList} resolutions All available resolutions.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.photoOptTextTempl_ = function(
    r, resolutions) {
  /**
   * @param {number} a
   * @param {number} b
   * @return {number}
   */
  const gcd = (a, b) => (a === 0 ? b : gcd(b % a, a));
  /**
   * @param {!Resolution} r
   * @return {number}
   */
  const toMegapixel = (r) => Math.round(r.area / 100000) / 10;
  const /** number */ d = gcd(r.width, r.height);
  if (resolutions.some(
          (findR) => !findR.equals(r) && r.aspectRatioEquals(findR) &&
              toMegapixel(r) === toMegapixel(findR))) {
    return chrome.i18n.getMessage(
        'label_detail_photo_resolution',
        [r.width / d, r.height / d, r.width, r.height, toMegapixel(r)]);
  } else {
    return chrome.i18n.getMessage(
        'label_photo_resolution', [r.width / d, r.height / d, toMegapixel(r)]);
  }
};

/**
 * Template for generating option text from video resolution width and height.
 * @param {!Resolution} r Resolution of text to be generated.
 * @return {string} Text shown on resolution option item.
 * @private
 */
cca.views.ResolutionSettings.prototype.videoOptTextTempl_ = function(r) {
  return chrome.i18n.getMessage(
      'label_video_resolution', [r.height, r.width].map(String));
};

/**
 * Finds photo and video resolution setting of target device id.
 * @param {string} deviceId
 * @return {?cca.views.DeviceSetting}
 * @private
 */
cca.views.ResolutionSettings.prototype.getDeviceSetting_ = function(deviceId) {
  if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
    return this.frontSetting_;
  }
  if (this.backSetting_ && this.backSetting_.deviceId === deviceId) {
    return this.backSetting_;
  }
  return this.externalSettings_.find((e) => e.deviceId === deviceId) || null;
};

/**
 * Updates resolution information of front, back camera and external cameras.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateResolutions_ = function() {
  /**
   * @param {!HTMLElement} item
   * @param {string} id
   * @param {!cca.views.ResolutionConfig} config
   * @param {!function(!Resolution, !ResolutionList): string} optTextTempl
   */
  const prepItem = (item, id, {prefResol, resols}, optTextTempl) => {
    item.dataset.deviceId = id;
    item.classList.toggle('multi-option', resols.length > 1);
    item.querySelector('.description>span').textContent =
        optTextTempl(prefResol, resols);
  };

  // Update front camera setting
  cca.state.set('has-front-camera', this.frontSetting_ !== null);
  if (this.frontSetting_) {
    const {deviceId, photo, video} = this.frontSetting_;
    prepItem(this.frontPhotoItem_, deviceId, photo, this.photoOptTextTempl_);
    prepItem(this.frontVideoItem_, deviceId, video, this.videoOptTextTempl_);
  }

  // Update back camera setting
  cca.state.set('has-back-camera', this.backSetting_ !== null);
  if (this.backSetting_) {
    const {deviceId, photo, video} = this.backSetting_;
    prepItem(this.backPhotoItem_, deviceId, photo, this.photoOptTextTempl_);
    prepItem(this.backVideoItem_, deviceId, video, this.videoOptTextTempl_);
  }

  // Update external camera settings
  // To prevent losing focus on item already exist before update, locate
  // focused item in both previous and current list, pop out all items in
  // previous list except those having same deviceId as focused one and
  // recreate all other items from current list.
  const prevFocus = /** @type {?HTMLElement} */ (
      this.resMenu_.querySelector('.menu-item.external-camera:focus'));
  /** @type {?string} */
  const prevFId = prevFocus && prevFocus.dataset.deviceId;
  const /** number */ focusIdx =
      this.externalSettings_.findIndex(({deviceId}) => deviceId === prevFId);
  const fTitle = /** @type {?HTMLElement} */ (this.resMenu_.querySelector(
      `.external-camera.title-item[data-device-id="${prevFId}"]`));
  /** @type {?string} */
  const focusedId = focusIdx === -1 ? null : prevFId;

  this.resMenu_.querySelectorAll('.menu-item.external-camera')
      .forEach(
          (element) => element.dataset.deviceId !== focusedId &&
              element.parentNode.removeChild(element));

  this.externalSettings_.forEach((config, index) => {
    const {deviceId} = config;
    let /** !HTMLElement */ titleItem;
    let /** !HTMLElement */ photoItem;
    let /** !HTMLElement */ videoItem;
    if (deviceId !== focusedId) {
      const extItem = /** @type {!HTMLElement} */ (
          document.importNode(this.extcamItemTempl_.content, true));
      cca.util.setupI18nElements(extItem);
      [titleItem, photoItem, videoItem] =
          /** @type {!NodeList<!HTMLElement>}*/ (
              extItem.querySelectorAll('.menu-item'));

      photoItem.addEventListener('click', () => {
        if (photoItem.classList.contains('multi-option')) {
          this.openPhotoResSettings_(config, photoItem);
        }
      });
      photoItem.setAttribute('aria-describedby', `${deviceId}-photores-desc`);
      photoItem.querySelector('.description').id = `${deviceId}-photores-desc`;
      videoItem.addEventListener('click', () => {
        if (videoItem.classList.contains('multi-option')) {
          this.openVideoResSettings_(config, videoItem);
        }
      });
      videoItem.setAttribute('aria-describedby', `${deviceId}-videores-desc`);
      videoItem.querySelector('.description').id = `${deviceId}-videores-desc`;
      if (index < focusIdx) {
        this.resMenu_.insertBefore(extItem, fTitle);
      } else {
        this.resMenu_.appendChild(extItem);
      }
    } else {
      titleItem = /** @type {!HTMLElement}*/ (fTitle);
      photoItem = /** @type {!HTMLElement}*/ (fTitle.nextElementSibling);
      videoItem = /** @type {!HTMLElement}*/ (photoItem.nextElementSibling);
    }
    titleItem.dataset.deviceId = deviceId;
    prepItem(photoItem, deviceId, config.photo, this.photoOptTextTempl_);
    prepItem(videoItem, deviceId, config.video, this.videoOptTextTempl_);
  });
  // Force closing opened setting of unplugged device.
  if ((cca.state.get('photoresolutionsettings') ||
       cca.state.get('videoresolutionsettings')) &&
      this.openedSettingDeviceId_ !== null &&
      this.getDeviceSetting_(this.openedSettingDeviceId_) !== undefined) {
    cca.nav.close(
        cca.state.get('photoresolutionsettings') ? 'photoresolutionsettings' :
                                                   'videoresolutionsettings');
  }
};

/**
 * Updates current selected photo resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {!Resolution} resolution Selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedPhotoResolution_ =
    function(deviceId, resolution) {
  const {photo} = this.getDeviceSetting_(deviceId);
  photo.prefResol = resolution;
  let /** !HTMLElement */ photoItem;
  if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
    photoItem = this.frontPhotoItem_;
  } else if (this.backsetting_ && this.backsetting_.deviceId === deviceId) {
    photoItem = this.backPhotoItem_;
  } else {
    photoItem = /** @type {!HTMLElement} */ (this.resMenu_.querySelector(
        `.menu-item.photo-item[data-device-id="${deviceId}"]`));
  }
  photoItem.querySelector('.description>span').textContent =
      this.photoOptTextTempl_(photo.prefResol, photo.resols);

  // Update setting option if it's opened.
  if (cca.state.get('photoresolutionsettings') &&
      this.openedSettingDeviceId_ === deviceId) {
    this.photoResMenu_
        .querySelector(
            'input' +
            `[data-width="${resolution.width}"]` +
            `[data-height="${resolution.height}"]`)
        .checked = true;
  }
};

/**
 * Updates current selected video resolution.
 * @param {string} deviceId Device id of the selected resolution.
 * @param {!Resolution} resolution Selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateSelectedVideoResolution_ =
    function(deviceId, resolution) {
  const {video} = this.getDeviceSetting_(deviceId);
  video.prefResol = resolution;
  let /** !HTMLElement */ videoItem;
  if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
    videoItem = this.frontVideoItem_;
  } else if (this.backSetting_ && this.backSetting_.deviceId === deviceId) {
    videoItem = this.backVideoItem_;
  } else {
    videoItem = /** @type {!HTMLElement} */ (this.resMenu_.querySelector(
        `.menu-item.video-item[data-device-id="${deviceId}"]`));
  }
  videoItem.querySelector('.description>span').textContent =
      this.videoOptTextTempl_(video.prefResol);

  // Update setting option if it's opened.
  if (cca.state.get('videoresolutionsettings') &&
      this.openedSettingDeviceId_ === deviceId) {
    this.videoResMenu_
        .querySelector(
            'input' +
            `[data-width="${resolution.width}"]` +
            `[data-height="${resolution.height}"]`)
        .checked = true;
  }
};

/**
 * Opens photo resolution setting view.
 * @param {!cca.views.DeviceSetting} Setting of video device to be opened.
 * @param {!HTMLElement} resolItem Dom element from upper layer menu item
 *     showing title of the selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.openPhotoResSettings_ = function(
    {deviceId, photo}, resolItem) {
  this.openedSettingDeviceId_ = deviceId;
  this.updateMenu_(
      resolItem, this.photoResMenu_, this.photoOptTextTempl_,
      (r) => this.photoPreferrer_.changePreferredResolution(deviceId, r),
      photo.resols, photo.prefResol);
  this.openSubSettings('photoresolutionsettings');
};

/**
 * Opens video resolution setting view.
 * @param {!cca.views.DeviceSetting} Setting of video device to be opened.
 * @param {!HTMLElement} resolItem Dom element from upper layer menu item
 *     showing title of the selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.openVideoResSettings_ = function(
    {deviceId, video}, resolItem) {
  this.openedSettingDeviceId_ = deviceId;
  this.updateMenu_(
      resolItem, this.videoResMenu_, this.videoOptTextTempl_,
      (r) => this.videoPreferrer_.changePreferredResolution(deviceId, r),
      video.resols, video.prefResol);
  this.openSubSettings('videoresolutionsettings');
};

/**
 * Updates resolution menu with specified resolutions.
 * @param {!HTMLElement} resolItem DOM element holding selected resolution.
 * @param {!HTMLElement} menu Menu holding all resolution option elements.
 * @param {!function(!Resolution, !ResolutionList): string} optTextTempl
 *     Template generating text content for each resolution option from its
 *     width and height.
 * @param {!function(!Resolution)} onChange Called when selected option changed
 *     with resolution of newly selected option.
 * @param {!ResolutionList} resolutions Resolutions of its width and height to
 *     be updated with.
 * @param {!Resolution} selectedR Selected resolution.
 * @private
 */
cca.views.ResolutionSettings.prototype.updateMenu_ = function(
    resolItem, menu, optTextTempl, onChange, resolutions, selectedR) {
  const captionText = resolItem.querySelector('.description>span');
  captionText.textContent = '';
  menu.querySelectorAll('.menu-item')
      .forEach((element) => element.parentNode.removeChild(element));

  resolutions.forEach((r) => {
    const item = /** @type {!HTMLElement} */ (
        document.importNode(this.resItemTempl_.content, true));
    const inputElement =
        /** @type {!HTMLElement} */ (item.querySelector('input'));
    item.querySelector('span').textContent = optTextTempl(r, resolutions);
    inputElement.name = menu.dataset.name;
    inputElement.dataset.width = r.width;
    inputElement.dataset.height = r.height;
    if (r.equals(selectedR)) {
      captionText.textContent = optTextTempl(r, resolutions);
      inputElement.checked = true;
    }
    inputElement.addEventListener('click', (event) => {
      if (!cca.state.get('streaming') || cca.state.get('taking')) {
        event.preventDefault();
      }
    });
    inputElement.addEventListener('change', (event) => {
      if (inputElement.checked) {
        captionText.textContent = optTextTempl(r, resolutions);
        onChange(r);
      }
    });
    menu.appendChild(item);
  });
};
