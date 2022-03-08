// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertInstanceof} from '../assert.js';
import {CameraConfig, CameraManager} from '../device/index.js';
import * as dom from '../dom.js';
import {reportError} from '../error.js';
import {setExpertMode} from '../expert.js';
import {I18nString} from '../i18n_string.js';
import * as loadTimeData from '../models/load_time_data.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import * as nav from '../nav.js';
import * as state from '../state.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  Resolution,
  ResolutionList,
  ViewName,
} from '../type.js';
import * as util from '../util.js';

import {LeaveCondition, View} from './view.js';

/**
 * Object of device id, preferred capture resolution and all
 * available resolutions for a particular video device.
 */
interface ResolutionConfig {
  prefResolution: Resolution|null;
  resolutions: Resolution[];
}

/**
 * Photo and video resolution configuration for a particular video device.
 */
interface DeviceSetting {
  deviceId: string;
  photo: ResolutionConfig;
  video: ResolutionConfig;
}

/**
 * Base controller of settings view.
 */
export class BaseSettings extends View {
  /**
   * The default focus element when focus on view is reset.
   */
  private readonly defaultFocus: HTMLElement;

  /**
   * The DOM element to be focused when the focus on view is reset by calling
   * |focus()|.
   */
  private focusElement: HTMLElement;

  /**
   * @param name Name of the view.
   * @param itemHandlers Click-handlers mapped by element ids.
   */
  constructor(
      name: ViewName,
      itemHandlers: Record<string, (event: Event) => void> = {}) {
    super(name, {dismissByEsc: true, dismissByBackgroundClick: true});

    dom.getFrom(this.root, '.menu-header button', HTMLButtonElement)
        .addEventListener('click', () => this.leave());
    for (const element of dom.getAllFrom(
             this.root, '.menu-item', HTMLElement)) {
      const handler = itemHandlers[element.id];
      if (handler !== undefined) {
        element.addEventListener('click', handler);
      }
    }

    this.defaultFocus = dom.getFrom(this.root, '[tabindex]', HTMLElement);

    this.focusElement = this.defaultFocus;
  }

  override focus(): void {
    this.focusElement.focus();
  }

  override leaving(condition: LeaveCondition): boolean {
    this.focusElement = this.defaultFocus;
    return super.leaving(condition);
  }

  /**
   * Opens sub-settings.
   *
   * @param opener The DOM element triggering the open.
   * @param name Name of settings view.
   */
  protected async openSubSettings(opener: HTMLElement, name: ViewName):
      Promise<void> {
    this.focusElement = opener;
    // Dismiss primary-settings if sub-settings was dismissed by background
    // click.
    const cond = await nav.open(name);
    if (cond.kind === 'BACKGROUND_CLICKED') {
      this.leave(cond);
    }
  }
}

const helpUrl =
    'https://support.google.com/chromebook/?p=camera_usage_on_chromebook';

/**
 * Controller of primary settings view.
 */
export class PrimarySettings extends BaseSettings {
  private readonly subViews: BaseSettings[];

  private headerClickedCount = 0;

  private headerClickedLastTime: number|null = null;

  constructor(cameraManager: CameraManager) {
    super(
        ViewName.SETTINGS,
        // Use an IIFE here since TypeScript doesn't allow any statement
        // before super() call if we have property initializers.
        (() => {
          const openHandler = (openerId: string, viewName: ViewName) => {
            const opener = dom.get(`#${openerId}`, HTMLElement);
            return {[openerId]: () => this.openSubSettings(opener, viewName)};
          };
          return {
            ...openHandler('settings-gridtype', ViewName.GRID_SETTINGS),
            ...openHandler('settings-timerdur', ViewName.TIMER_SETTINGS),
            ...openHandler('settings-resolution', ViewName.RESOLUTION_SETTINGS),
            ...openHandler('settings-expert', ViewName.EXPERT_SETTINGS),
            'settings-feedback': () => {
              // Prevent setting view overlapping preview when sending app
              // window feedback screenshot b/155938542.
              this.leave();
              ChromeHelper.getInstance().openFeedbackDialog(
                  loadTimeData.getI18nMessage(
                      I18nString.FEEDBACK_DESCRIPTION_PLACEHOLDER));
            },
            'settings-help': () => {
              ChromeHelper.getInstance().openUrlInBrowser(helpUrl);
            },
          };
        })(),
    );

    this.subViews = [
      new BaseSettings(ViewName.GRID_SETTINGS),
      new BaseSettings(ViewName.TIMER_SETTINGS),
      new ResolutionSettings(cameraManager),
      new BaseSettings(ViewName.EXPERT_SETTINGS),
    ];

    const header = dom.get('#settings-header', HTMLElement);
    header.addEventListener('click', () => this.onHeaderClicked());
  }

  /**
   * Handle click on primary settings header (used to trigger expert mode).
   */
  private onHeaderClicked() {
    const reset = () => {
      this.headerClickedCount = 0;
      this.headerClickedLastTime = null;
    };

    // Reset the counter if last click is more than 1 second ago.
    if (this.headerClickedLastTime !== null &&
        (Date.now() - this.headerClickedLastTime) > 1000) {
      reset();
    }

    this.headerClickedCount++;
    this.headerClickedLastTime = Date.now();

    if (this.headerClickedCount === 5) {
      setExpertMode(true);
      reset();
    }
  }

  override getSubViews(): View[] {
    return this.subViews;
  }
}

/**
 * Controller of resolution settings view.
 */
export class ResolutionSettings extends BaseSettings {
  /**
   * Device setting of front camera. Null if no front camera.
   */
  private frontSetting: DeviceSetting|null = null;

  private readonly frontPhotoItem =
      dom.get('#settings-front-photores', HTMLElement);

  private readonly frontVideoItem =
      dom.get('#settings-front-videores', HTMLElement);

  /**
   * Device setting of back camera. Null if no back camera.
   */
  private backSetting: DeviceSetting|null = null;

  private readonly backPhotoItem =
      dom.get('#settings-back-photores', HTMLElement);

  private readonly backVideoItem =
      dom.get('#settings-back-videores', HTMLElement);

  readonly photoResolutionSettings =
      new BaseSettings(ViewName.PHOTO_RESOLUTION_SETTINGS);

  readonly videoResolutionSettings =
      new BaseSettings(ViewName.VIDEO_RESOLUTION_SETTINGS);

  private readonly resolutionMenu: HTMLDivElement;

  private readonly videoResolutionMenu: HTMLDivElement;

  private readonly photoResolutionMenu: HTMLDivElement;

  private cameraAvailable = false;

  /**
   * Device setting of external cameras.
   */
  private externalSettings: DeviceSetting[] = [];

  /**
   * Device id of currently opened resolution setting view.
   */
  private openedSettingDeviceId: string|null = null;

  constructor(readonly cameraManager: CameraManager) {
    super(
        ViewName.RESOLUTION_SETTINGS,
        // Use an IIFE here since TypeScript doesn't allow any statement
        // before super() call if we have property initializers.
        (() => {
          const createOpenMenuHandler =
              (getSetting: () => DeviceSetting | null,
               getElement: () => HTMLElement, isPhoto: boolean) => () => {
                const setting = getSetting();
                if (setting === null) {
                  reportError(
                      ErrorType.DEVICE_NOT_EXIST, ErrorLevel.ERROR,
                      new Error('Open settings of non-exist device.'));
                  return;
                }
                const element = getElement();
                if (element.classList.contains('multi-option')) {
                  if (isPhoto) {
                    this.openPhotoResSettings(setting, element);
                  } else {
                    this.openVideoResSettings(setting, element);
                  }
                }
              };
          return {
            'settings-front-photores': createOpenMenuHandler(
                () => this.frontSetting, () => this.frontPhotoItem, true),
            'settings-front-videores': createOpenMenuHandler(
                () => this.frontSetting, () => this.frontVideoItem, false),
            'settings-back-photores': createOpenMenuHandler(
                () => this.backSetting, () => this.backPhotoItem, true),
            'settings-back-videores': createOpenMenuHandler(
                () => this.backSetting, () => this.backVideoItem, false),
          };
        })(),
    );

    this.resolutionMenu = dom.getFrom(this.root, 'div.menu', HTMLDivElement);

    this.videoResolutionMenu = dom.getFrom(
        this.videoResolutionSettings.root, 'div.menu', HTMLDivElement);

    this.photoResolutionMenu = dom.getFrom(
        this.photoResolutionSettings.root, 'div.menu', HTMLDivElement);

    state.addObserver(state.State.TAKING, () => {
      this.updateOptionAvailability();
    });

    cameraManager.registerCameraUI({
      onCameraUnavailable: () => {
        if (state.get(state.State.NO_RESOLUTION_SETTINGS)) {
          return;
        }
        this.cameraAvailable = false;
        this.updateOptionAvailability();
      },
      onCameraAvailble: () => {
        if (state.get(state.State.NO_RESOLUTION_SETTINGS)) {
          return;
        }
        this.cameraAvailable = true;
        this.updateOptionAvailability();
      },
      onUpdateCapability: (cameraInfo) => {
        const devices = cameraInfo.camera3DevicesInfo;
        if (devices === null) {
          return;
        }
        state.set(state.State.NO_RESOLUTION_SETTINGS, false);
        this.externalSettings = [];

        for (const {deviceId, facing, photoResolutions, videoResolutions} of
                 devices) {
          const deviceSetting = {
            deviceId,
            photo: {
              prefResolution: cameraManager.getPrefPhotoResolution(deviceId),
              resolutions:
                  /* Filter out resolutions of megapixels < 0.1 i.e.
                   * megapixels 0.0 */
                  photoResolutions.filter((r) => r.area >= 100000),
            },
            video: {
              prefResolution: cameraManager.getPrefVideoResolution(deviceId),
              resolutions: videoResolutions,
            },
          };
          switch (facing) {
            case Facing.USER:
              this.frontSetting = deviceSetting;
              break;
            case Facing.ENVIRONMENT:
              this.backSetting = deviceSetting;
              break;
            case Facing.EXTERNAL:
              this.externalSettings.push(deviceSetting);
              break;
            default:
              reportError(
                  ErrorType.UNKNOWN_FACING, ErrorLevel.ERROR,
                  new Error(`Ignore device of unknown facing: ${facing}`));
          }
        }
        this.updateResolutions();
      },
      onUpdateConfig: (config: CameraConfig) => {
        if (state.get(state.State.NO_RESOLUTION_SETTINGS)) {
          return;
        }
        const prefResolution = cameraManager.getCaptureResolution();
        if (prefResolution !== null) {
          if (config.mode === Mode.VIDEO) {
            this.updateSelectedVideoResolution(config.deviceId, prefResolution);
          } else {
            this.updateSelectedPhotoResolution(config.deviceId, prefResolution);
          }
        }
      },
    });
  }

  private updateOptionAvailability(): void {
    for (const e of dom.getAll('.resolution-option>input', HTMLInputElement)) {
      e.disabled = !this.cameraAvailable || state.get(state.State.TAKING);
    }
  }


  override getSubViews(): View[] {
    return [
      this.photoResolutionSettings,
      this.videoResolutionSettings,
    ];
  }

  /**
   * Template for generating option text from photo resolution width and height.
   *
   * @param resolution Resolution of text to be generated.
   * @param resolutions All available resolutions.
   * @return Text shown on resolution option item.
   */
  private photoOptionTextTemplate(
      resolution: Resolution|null, resolutions: Resolution[]): string {
    if (resolution === null) {
      return '';
    }
    function gcd(a: number, b: number): number {
      return a === 0 ? b : gcd(b % a, a);
    }
    function toMegapixel({area}: Resolution): number {
      return area >= 1e6 ? Math.round(area / 1e6) : Math.round(area / 1e5) / 10;
    }
    const d = gcd(resolution.width, resolution.height);

    if (resolutions.some(
            (r) => !r.equals(resolution) && resolution.aspectRatioEquals(r) &&
                toMegapixel(resolution) === toMegapixel(r))) {
      return loadTimeData.getI18nMessage(
          I18nString.LABEL_DETAIL_PHOTO_RESOLUTION, resolution.width / d,
          resolution.height / d, resolution.width, resolution.height,
          toMegapixel(resolution));
    } else {
      return loadTimeData.getI18nMessage(
          I18nString.LABEL_PHOTO_RESOLUTION, resolution.width / d,
          resolution.height / d, toMegapixel(resolution));
    }
  }

  /**
   * Template for generating option text from video resolution width and height.
   *
   * @param resolution Resolution of text to be generated.
   * @return Text shown on resolution option item.
   */
  private videoOptionTextTemplate(resolution: Resolution|null): string {
    if (resolution === null) {
      return '';
    }
    return loadTimeData.getI18nMessage(
        I18nString.LABEL_VIDEO_RESOLUTION, resolution.height, resolution.width);
  }

  /**
   * Finds photo and video resolution setting of target device id.
   */
  private getDeviceSetting(deviceId: string): DeviceSetting|null {
    if (this.frontSetting && this.frontSetting.deviceId === deviceId) {
      return this.frontSetting;
    }
    if (this.backSetting && this.backSetting.deviceId === deviceId) {
      return this.backSetting;
    }
    return this.externalSettings.find((e) => e.deviceId === deviceId) ?? null;
  }

  /**
   * Updates resolution information of front, back camera and external cameras.
   */
  private updateResolutions() {
    function prepareItem(
        item: HTMLElement, id: string,
        {prefResolution, resolutions}: ResolutionConfig,
        optionTextTemplate:
            (prefResolutions: Resolution|null, resolutions: Resolution[]) =>
                string) {
      item.dataset['deviceId'] = id;
      item.classList.toggle('multi-option', resolutions.length > 1);
      dom.getFrom(item, '.description>span', HTMLSpanElement).textContent =
          optionTextTemplate(prefResolution, resolutions);
    }

    // Update front camera setting
    state.set(state.State.HAS_FRONT_CAMERA, this.frontSetting !== null);
    if (this.frontSetting !== null) {
      const {deviceId, photo, video} = this.frontSetting;
      prepareItem(
          this.frontPhotoItem, deviceId, photo, this.photoOptionTextTemplate);
      prepareItem(
          this.frontVideoItem, deviceId, video, this.videoOptionTextTemplate);
    }

    // Update back camera setting
    state.set(state.State.HAS_BACK_CAMERA, this.backSetting !== null);
    if (this.backSetting !== null) {
      const {deviceId, photo, video} = this.backSetting;
      prepareItem(
          this.backPhotoItem, deviceId, photo, this.photoOptionTextTemplate);
      prepareItem(
          this.backVideoItem, deviceId, video, this.videoOptionTextTemplate);
    }

    // Update external camera settings
    // To prevent losing focus on item already exist before update, locate
    // focused item in both previous and current list, pop out all items in
    // previous list except those having same deviceId as focused one and
    // recreate all other items from current list.
    const prevFocused = this.resolutionMenu.querySelector<HTMLElement>(
        '.menu-item.external-camera:focus');
    const prevFocusedId = prevFocused?.dataset['deviceId'] ?? null;
    const focusedIdx = this.externalSettings.findIndex(
        ({deviceId}) => deviceId === prevFocusedId);
    const prevFocusedTitle = this.resolutionMenu.querySelector<HTMLElement>(
        `.external-camera.title-item[data-device-id="${prevFocusedId}"]`);
    const focusedId = focusedIdx === -1 ? null : prevFocusedId;

    for (const element of dom.getAllFrom(
             this.resolutionMenu, '.menu-item.external-camera', HTMLElement)) {
      if (element.dataset['deviceId'] !== focusedId) {
        assertExists(element.parentNode).removeChild(element);
      }
    }

    for (const [index, config] of this.externalSettings.entries()) {
      const {deviceId} = config;
      let titleItem: HTMLElement;
      let photoItem: HTMLElement;
      let videoItem: HTMLElement;
      if (deviceId !== focusedId) {
        const extItem =
            util.instantiateTemplate('#extcam-resolution-item-template');
        [titleItem, photoItem, videoItem] =
            dom.getAllFrom(extItem, '.menu-item', HTMLElement);

        photoItem.addEventListener('click', () => {
          if (photoItem.classList.contains('multi-option')) {
            this.openPhotoResSettings(config, photoItem);
          }
        });
        photoItem.setAttribute('aria-describedby', `${deviceId}-photores-desc`);
        dom.getFrom(photoItem, '.description', HTMLElement).id =
            `${deviceId}-photores-desc`;
        videoItem.addEventListener('click', () => {
          if (videoItem.classList.contains('multi-option')) {
            this.openVideoResSettings(config, videoItem);
          }
        });
        videoItem.setAttribute('aria-describedby', `${deviceId}-videores-desc`);
        dom.getFrom(videoItem, '.description', HTMLElement).id =
            `${deviceId}-videores-desc`;
        if (index < focusedIdx) {
          this.resolutionMenu.insertBefore(extItem, prevFocusedTitle);
        } else {
          this.resolutionMenu.appendChild(extItem);
        }
      } else {
        assert(prevFocusedTitle !== null);
        titleItem = prevFocusedTitle;
        photoItem =
            assertInstanceof(prevFocusedTitle.nextElementSibling, HTMLElement);
        videoItem = assertInstanceof(photoItem.nextElementSibling, HTMLElement);
      }
      titleItem.dataset['deviceId'] = deviceId;
      prepareItem(
          photoItem, deviceId, config.photo, this.photoOptionTextTemplate);
      prepareItem(
          videoItem, deviceId, config.video, this.videoOptionTextTemplate);
    }
    // Force closing opened setting of unplugged device.
    if ((state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) ||
         state.get(ViewName.VIDEO_RESOLUTION_SETTINGS)) &&
        this.openedSettingDeviceId !== null &&
        this.getDeviceSetting(this.openedSettingDeviceId) === null) {
      nav.close(
          state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) ?
              ViewName.PHOTO_RESOLUTION_SETTINGS :
              ViewName.VIDEO_RESOLUTION_SETTINGS);
    }
  }

  /**
   * Updates current selected photo resolution.
   *
   * @param deviceId Device id of the selected resolution.
   * @param resolution Selected resolution.
   */
  private updateSelectedPhotoResolution(
      deviceId: string, resolution: Resolution) {
    const {photo} = assertExists(this.getDeviceSetting(deviceId));
    photo.prefResolution = resolution;
    let photoItem: HTMLElement;
    if (this.frontSetting && this.frontSetting.deviceId === deviceId) {
      photoItem = this.frontPhotoItem;
    } else if (this.backSetting && this.backSetting.deviceId === deviceId) {
      photoItem = this.backPhotoItem;
    } else {
      photoItem = dom.getFrom(
          this.resolutionMenu,
          `.menu-item.photo-item[data-device-id="${deviceId}"]`, HTMLElement);
    }
    dom.getFrom(photoItem, '.description>span', HTMLSpanElement).textContent =
        this.photoOptionTextTemplate(photo.prefResolution, photo.resolutions);

    // Update setting option if it's opened.
    if (state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) &&
        this.openedSettingDeviceId === deviceId) {
      const input = dom.getFrom(
          this.photoResolutionMenu,
          'input' +
              `[data-width="${resolution.width}"]` +
              `[data-height="${resolution.height}"]`,
          HTMLInputElement);
      input.checked = true;
    }
  }

  /**
   * Updates current selected video resolution.
   *
   * @param deviceId Device id of the selected resolution.
   * @param resolution Selected resolution.
   */
  private updateSelectedVideoResolution(
      deviceId: string, resolution: Resolution) {
    const {video} = assertExists(this.getDeviceSetting(deviceId));
    video.prefResolution = resolution;
    let videoItem: HTMLElement;
    if (this.frontSetting && this.frontSetting.deviceId === deviceId) {
      videoItem = this.frontVideoItem;
    } else if (this.backSetting && this.backSetting.deviceId === deviceId) {
      videoItem = this.backVideoItem;
    } else {
      videoItem = dom.getFrom(
          this.resolutionMenu,
          `.menu-item.video-item[data-device-id="${deviceId}"]`, HTMLElement);
    }
    dom.getFrom(videoItem, '.description>span', HTMLSpanElement).textContent =
        this.videoOptionTextTemplate(video.prefResolution);

    // Update setting option if it's opened.
    if (state.get(ViewName.VIDEO_RESOLUTION_SETTINGS) &&
        this.openedSettingDeviceId === deviceId) {
      const input = dom.getFrom(
          this.videoResolutionMenu,
          'input' +
              `[data-width="${resolution.width}"]` +
              `[data-height="${resolution.height}"]`,
          HTMLInputElement);
      input.checked = true;
    }
  }

  /**
   * Opens photo resolution setting view.
   *
   * @param setting Setting of video device to be opened.
   * @param resolutionItem Dom element from upper layer menu item showing title
   *     of the selected resolution.
   */
  private openPhotoResSettings(
      setting: DeviceSetting, resolutionItem: HTMLElement) {
    const {deviceId, photo} = setting;
    this.openedSettingDeviceId = deviceId;
    this.updateMenu(
        resolutionItem, this.photoResolutionMenu, this.photoOptionTextTemplate,
        (r) => this.cameraManager.setPrefPhotoResolution(deviceId, r),
        photo.resolutions, photo.prefResolution);
    this.openSubSettings(resolutionItem, ViewName.PHOTO_RESOLUTION_SETTINGS);
  }

  /**
   * Opens video resolution setting view.
   *
   * @param setting Setting of video device to be opened.
   * @param resolutionItem Dom element from upper layer menu item showing title
   *     of the selected resolution.
   */
  private openVideoResSettings(
      setting: DeviceSetting, resolutionItem: HTMLElement) {
    const {deviceId, video} = setting;
    this.openedSettingDeviceId = deviceId;
    this.updateMenu(
        resolutionItem, this.videoResolutionMenu, this.videoOptionTextTemplate,
        (r) => this.cameraManager.setPrefVideoResolution(deviceId, r),
        video.resolutions, video.prefResolution);
    this.openSubSettings(resolutionItem, ViewName.VIDEO_RESOLUTION_SETTINGS);
  }

  /**
   * Updates resolution menu with specified resolutions.
   *
   * @param resolutionItem DOM element holding selected resolution.
   * @param menu Menu holding all resolution option elements.
   * @param optionTextTemplate Template generating text content for each
   *     resolution option from its width and height.
   * @param onChange Called when selected option changed with resolution of
   *     newly selected option.
   * @param resolutions Resolutions of its width and height to be updated with.
   * @param selectedResolution Selected resolution.
   */
  private updateMenu(
      resolutionItem: HTMLElement,
      menu: HTMLElement,
      optionTextTemplate:
          (resolution: Resolution, resolutions: ResolutionList) => string,
      onChange: (resolution: Resolution) => void,
      resolutions: ResolutionList,
      selectedResolution: Resolution|null,
  ) {
    const captionText =
        dom.getFrom(resolutionItem, '.description>span', HTMLSpanElement);
    captionText.textContent = '';
    for (const element of dom.getAllFrom(
             menu, '.menu-item', HTMLLabelElement)) {
      assertExists(element.parentNode).removeChild(element);
    }

    for (const resolution of resolutions) {
      const item = util.instantiateTemplate('#resolution-item-template');
      const input = dom.getFrom(item, 'input', HTMLInputElement);
      dom.getFrom(item, 'span', HTMLSpanElement).textContent =
          optionTextTemplate(resolution, resolutions);
      input.name = assertExists(menu.dataset[I18nString.NAME]);
      input.dataset['width'] = resolution.width.toString();
      input.dataset['height'] = resolution.height.toString();
      if (selectedResolution?.equals(resolution)) {
        captionText.textContent = optionTextTemplate(resolution, resolutions);
        input.checked = true;
      }
      input.disabled = state.get(state.State.CAMERA_CONFIGURING) ||
          state.get(state.State.TAKING);
      input.addEventListener('change', () => {
        if (input.checked) {
          captionText.textContent = optionTextTemplate(resolution, resolutions);
          onChange(resolution);
        }
      });
      menu.appendChild(item);
    }
  }
}
