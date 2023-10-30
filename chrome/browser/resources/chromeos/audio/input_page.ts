// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getRequiredElement} from 'chrome://resources/js/util.js';

import {AudioBroker} from './audio_broker.js';
import {Page, PageNavigator} from './page.js';


export class InputPage extends Page {
  testInputFeedback: Map<string, null|string>;
  private analyserLeft?: AnalyserNode;
  private analyserRight?: AnalyserNode;
  private animationRequestId?: number;
  private recordClicked: boolean;
  private audioContext: AudioContext|null;
  private mediaRecorder: MediaRecorder|null;
  private intervalId: number|null;

  constructor() {
    super('input');
    this.audioContext = null;
    this.mediaRecorder = null;
    this.recordClicked = false;
    this.intervalId = null;
    this.testInputFeedback =
        new Map([['audioUrl', null], ['Can Hear Clearly', null]]);
    this.setUpButtons();
  }

  override showPage() {
    super.showPage();
    if (this.audioContext) {
      this.audioContext.resume();
    } else {
      this.initAudio(true);
    }
  }

  override hidePage() {
    super.hidePage();
    if (this.audioContext) {
      this.audioContext.suspend();
    }
    if (this.recordClicked) {
      this.stopRecord();
    }
  }

  updateActiveInputDevice() {
    const handler = AudioBroker.getInstance().handler;
    handler.getActiveInputDeviceName().then(({deviceName}) => {
      getRequiredElement('active-input').innerHTML =
          deviceName ?? 'No active input device';
    });
  }

  visualize() {
    const pairs: Array<{
      canvas: HTMLCanvasElement,
      analyser: AnalyserNode | undefined,
    }> =
        [
          {
            canvas: getRequiredElement<HTMLCanvasElement>('channel-l'),
            analyser: this.analyserLeft,
          },
          {
            canvas: getRequiredElement<HTMLCanvasElement>('channel-r'),
            analyser: this.analyserRight,
          },
        ];
    const draw = () => {
      this.animationRequestId = requestAnimationFrame(draw);
      for (const channel of pairs) {
        const canvas = channel['canvas'];
        const canvasContext = canvas.getContext('2d');
        const analyser = channel['analyser'];

        if (canvasContext && analyser) {
          analyser.fftSize = 2048;
          const bufferSize = analyser.frequencyBinCount;
          const buffer = new Uint8Array(bufferSize);

          /* Since we are using percentage for width and height, make it the
           * real value */
          canvas.width = canvas.clientWidth;
          canvas.height = canvas.clientHeight;

          const width = canvas.clientWidth;
          const height = canvas.clientHeight;

          analyser.getByteTimeDomainData(buffer);
          canvasContext.fillStyle = 'rgb(200, 200, 200)';
          canvasContext.fillRect(0, 0, width, height);
          canvasContext.lineWidth = 2;
          canvasContext.strokeStyle = 'rgb(0, 0, 0)';

          canvasContext.beginPath();

          const dx = width * 1.0 / bufferSize;
          let x = 0;

          for (let i = 0; i < bufferSize; i++) {
            const data = buffer[i];
            if (data) {
              const v = data / 128.0;
              const y = v * height / 2;
              if (i === 0) {
                canvasContext.moveTo(x, y);
              } else {
                canvasContext.lineTo(x, y);
              }
              x += dx;
            }
          }
          canvasContext.lineTo(width, height / 2);
          canvasContext.stroke();
        }
      }
    };
    if (this.animationRequestId) {
      window.cancelAnimationFrame(this.animationRequestId);
    }
    draw();
  }

  buildAudioGraph(source: MediaStreamAudioSourceNode) {
    if (this.audioContext) {
      const splitter = this.audioContext.createChannelSplitter(2);
      source.connect(splitter);
      this.analyserLeft = this.audioContext.createAnalyser();
      this.analyserRight = this.audioContext.createAnalyser();
      splitter.connect(this.analyserLeft, 0);
      splitter.connect(this.analyserRight, 1);
    }
  }

  initAudio(audioConstraint: boolean|Object) {
    this.audioContext = new window.AudioContext();
    navigator.mediaDevices.getUserMedia({'audio': audioConstraint})
        .then((streamGot) => {
          if (this.audioContext) {
            const stream = streamGot;
            const source = this.audioContext.createMediaStreamSource(stream);
            this.record(stream);
            this.buildAudioGraph(source);
            this.visualize();
          }
        });
  }

  record(source: MediaStream) {
    let chunks: Blob[] = [];
    const recordButton = getRequiredElement('record-btn');
    const clipSection = getRequiredElement('audio-file');
    this.mediaRecorder = new MediaRecorder(source);

    recordButton.onclick = () => {
      if (!this.recordClicked) {
        this.startRecord();
      } else {
        this.stopRecord();
      }
    };
    if (this.mediaRecorder) {
      this.mediaRecorder.onstop = () => {
        const clipContainer = document.createElement('article');
        const audio = document.createElement('audio');

        audio.setAttribute('controls', '');
        clipContainer.appendChild(audio);
        clipSection.appendChild(clipContainer);

        audio.controls = true;
        const blob = new Blob(chunks, {'type': 'audio/ogg; codecs=opus'});
        chunks = [];
        const audioURL = window.URL.createObjectURL(blob);
        audio.src = audioURL;
        this.testInputFeedback.set('audioUrl', audioURL);
      };

      this.mediaRecorder.ondataavailable = (event: BlobEvent) => {
        chunks.push(event.data);
      };
    }
  }

  startRecord() {
    if (this.mediaRecorder) {
      const recordButton = getRequiredElement('record-btn');
      const clipSection = getRequiredElement('audio-file');
      this.recordClicked = true;
      this.mediaRecorder.start();
      this.startTimer();
      recordButton.className = 'on-stop';
      recordButton.textContent = 'Stop';
      if (clipSection.firstChild) {
        clipSection.removeChild(clipSection.firstChild);
      }
    }
  }

  stopRecord() {
    if (this.mediaRecorder) {
      const recordButton = getRequiredElement('record-btn');
      this.recordClicked = false;
      this.mediaRecorder.stop();
      this.stopTimer();
      recordButton.className = 'on-record';
      recordButton.textContent = 'Record';
      getRequiredElement('input-qs').hidden = false;
    }
  }

  startTimer() {
    const startTime = Date.now();
    this.intervalId = window.setInterval(() => {
      const delta = Date.now() - startTime;
      getRequiredElement('counter').innerHTML =
          String(Math.floor(delta / 1000)) + ':' + String(delta % 1000);
    }, 200);
  }

  stopTimer() {
    if (this.intervalId) {
      clearInterval(this.intervalId);
      getRequiredElement('counter').innerHTML = '';
    }
  }

  setUpButtons() {
    getRequiredElement('input-yes').addEventListener('click', () => {
      this.testInputFeedback.set('Can Hear Clearly', 'true');
      PageNavigator.getInstance().showPage('feedback');
    });
    getRequiredElement('input-no').addEventListener('click', () => {
      this.testInputFeedback.set('Can Hear Clearly', 'false');
      PageNavigator.getInstance().showPage('feedback');
    });
  }


  static getInstance() {
    if (instance === null) {
      instance = new InputPage();
    }
    return instance;
  }
}

let instance: InputPage|null = null;
