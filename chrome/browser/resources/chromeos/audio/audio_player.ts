// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import {AudioSample, OutputPage} from './output_page.js';
import {PageNavigator} from './page.js';

export class AudioPlayer extends HTMLElement {
  private sampleIdx: number;
  private audioDiv: HTMLDivElement;
  private audioPlay: HTMLButtonElement;
  private audioContext: AudioContext|null;
  private audioQuery: HTMLDivElement;
  private audioNameTag: HTMLParagraphElement;
  private audioExpectation: HTMLParagraphElement;
  private prevLink: HTMLButtonElement;
  private timerId: number|null;
  constructor(private audioSamples: AudioSample[]) {
    super();
    this.sampleIdx = 0;
    this.audioContext = null;
    this.timerId = null;
    const clone =
        getRequiredElement<HTMLTemplateElement>('audioPlayer-template')
                      .content.cloneNode(true);
    this.audioDiv = (clone as HTMLElement).querySelector('div')!;
    this.audioPlay = this.audioDiv.querySelector('#play-btn')!;
    this.audioQuery = this.audioDiv.querySelector('#output-qs')!;
    this.audioNameTag = this.audioDiv.querySelectorAll('p')[0]!;
    this.audioExpectation = this.audioDiv.querySelectorAll('p')[1]!;
    this.prevLink = this.audioDiv.querySelector('#back')!;
    this.prevLink.textContent = '< Back';
    this.prevLink.addEventListener('click', () => {
      this.handleBackClick();
    });

    this.setUpAudioPlayer();
    this.setButtons();
    this.appendChild(this.audioDiv);
  }

  private get current() {
    return this.audioSamples[this.sampleIdx];
  }

  setUpAudioPlayer() {
    this.audioNameTag.innerHTML = `Playing: ${this.current!.description}`;
    this.setAudioExpectation();
  }

  setAudioExpectation() {
    if (this.current!.pan === -1) {
      this.audioExpectation.innerHTML =
          'Should hear audio coming from the left channel.';
    } else if (this.current!.pan === 1) {
      this.audioExpectation.innerHTML =
          'Should hear audio coming from the right channel.';
    } else {
      this.audioExpectation.innerHTML =
          'Should hear audio of a single pitch from all channels.';
    }
  }

  setButtons() {
    const yesLink = this.audioQuery.querySelector('#output-yes')!;
    const noLink = this.audioQuery.querySelector('#output-no')!;

    yesLink.addEventListener('click', () => this.handleResponse(true));
    noLink.addEventListener('click', () => this.handleResponse(false));

    this.audioPlay.addEventListener('click', () => {
      if (this.audioContext?.state === 'running') {
        this.audioContext.suspend();
      }

      this.audioContext =
          new AudioContext({sampleRate: this.current!.sampleRate});
      const oscNode = this.audioContext.createOscillator();
      oscNode.type = 'sine';
      oscNode.channelCount = this.current!.channelCount;
      oscNode.frequency.value = this.current!.freqency;
      if (this.current!.channelCount === 2) {
        const panNode = this.audioContext.createStereoPanner();
        panNode.pan.value = this.current!.pan;
        oscNode.connect(panNode);
        panNode.connect(this.audioContext.destination);
      } else {
        oscNode.connect(this.audioContext.destination);
      }
      if (this.timerId) {
        window.clearTimeout(this.timerId);
      }
      this.timerId = window.setTimeout(() => {
        this.audioContext?.suspend();
        this.audioQuery.hidden = false;
        this.timerId = null;
      }, 3000);
      oscNode.start();
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
    this.sampleIdx -= 1;
    this.audioContext?.suspend();
    if (this.timerId) {
      window.clearTimeout(this.timerId);
      this.timerId = null;
    }
    this.setUpAudioPlayer();
    this.prevLink.hidden = this.sampleIdx === 0;
    this.audioQuery.hidden = true;
  }

  handleResponse(response: boolean) {
    OutputPage.getInstance().setOutputMapEntry(this.current!, response);
    if (this.sampleIdx + 1 === this.audioSamples.length) {
      PageNavigator.getInstance().showPage('feedback');
    } else {
      this.sampleIdx += 1;
      this.setUpAudioPlayer();
      this.audioQuery.hidden = true;
      this.prevLink.hidden = this.sampleIdx === 0;
    }
  }
}
customElements.define('audio-player', AudioPlayer);
