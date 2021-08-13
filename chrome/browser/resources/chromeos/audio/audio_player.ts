// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';
import {OutputPage} from './output_page.js';
import {PageNavigator} from './page.js';

export class AudioPlayer extends HTMLElement {
  private current: string;
  private prev: string|undefined;
  private next: string|undefined;
  private audioDiv: HTMLDivElement;
  private audio: HTMLAudioElement;
  private audioNameTag: HTMLParagraphElement;
  private audioExpectation: HTMLParagraphElement;
  constructor(current: string, prev: string|undefined, next: string|undefined) {
    super();
    this.current = current;
    this.prev = prev;
    this.next = next;
    const clone = (<HTMLTemplateElement>$('audioPlayer-template'))
                      .content.cloneNode(true);
    this.audioDiv = <HTMLDivElement>(<HTMLElement>clone).querySelector('div');
    this.audio = <HTMLAudioElement>(this.audioDiv.querySelector('audio'));
    this.audioNameTag =
        <HTMLParagraphElement>(this.audioDiv.querySelectorAll('p')[0]);
    this.audioExpectation =
        <HTMLParagraphElement>(this.audioDiv.querySelectorAll('p')[1]);
    this.setUpAudioPlayer();
    this.appendChild(this.audioDiv);
  }

  setUpAudioPlayer() {
    this.hidden = true;
    this.id = this.current;
    this.audio.src = this.current;
    this.audioNameTag.innerHTML =
        'Playing from: ' + this.current.replace('sound/', '');
    this.createAudioExpectation();
    this.createButtons();
  }

  createAudioExpectation() {
    if (this.current.indexOf('left') !== -1) {
      this.audioExpectation.innerHTML =
          'Should hear audio coming from the left channel.';
    } else if (this.current.indexOf('right') !== -1) {
      this.audioExpectation.innerHTML =
          'Should hear audio coming from the right channel.';
    } else if (this.current.indexOf('1k') !== -1) {
      this.audioExpectation.innerHTML =
          'Warning: Should hear a high frequency pitch from all channels.';
    } else {
      this.audioExpectation.innerHTML =
          'Should hear audio of a single pitch from all channels.';
    }
  }

  createButtons() {
    if (this.prev) {
      const prevLink = this.createButton('back', '< Back');
      prevLink.addEventListener('click', () => {
        this.handleBackClick();
      });
    }
    const yesLink = this.createButton('yes', 'Yes');
    const noLink = this.createButton('no', 'No');

    yesLink.addEventListener('click', () => {
      yesLink.style.color = 'blue';
      noLink.style.color = 'black';
      this.handleResponse(true);
    });

    noLink.addEventListener('click', () => {
      noLink.style.color = 'blue';
      yesLink.style.color = 'black';
      this.handleResponse(false);
    });
  }

  createButton(buttonName: string, buttonText: string) {
    const button = document.createElement('button');
    const buttonClassName = buttonName + '-btn';
    button.setAttribute('class', buttonClassName);
    button.textContent = buttonText;
    this.audioDiv.appendChild(button);
    return button;
  }

  handleBackClick() {
    this.audio.pause();
    this.audio.currentTime = 0;
    if (this.prev) {
      this.hidden = true;
      $(this.prev).hidden = false;
    }
  }

  handleResponse(response: boolean) {
    OutputPage.getInstance().setOutputMapEntry(this.current, response);
    if (this.next) {
      this.handleNext();
    } else {
      PageNavigator.getInstance().showPage('feedback');
    }
  }

  handleNext() {
    this.audio.pause();
    this.audio.currentTime = 0;
    if (this.next) {
      this.hidden = true;
      $(this.next).hidden = false;
    }
  }
}
customElements.define('audio-player', AudioPlayer);
