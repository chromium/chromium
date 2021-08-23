import {$} from 'chrome://resources/js/util.m.js';

import {AudioBroker} from './audio_broker.js';
import {AudioPlayer} from './audio_player.js';
import {Page} from './page.js';

const AUDIOS = [
  'sound/mono_44k_sine440.wav', 'sound/mono_48k_sine440.wav',
  'sound/stereo_48k_sine440.wav', 'sound/stereo_48k_sine440_left.wav',
  'sound/stereo_48k_sine440_right.wav', 'sound/mono_48k_sine1k.wav'
];

export class OutputPage extends Page {
  testOutputFeedback: Map<string, null|boolean>;
  constructor() {
    super('output');
    this.testOutputFeedback = new Map();
    this.initOutputMap();
    this.createAudioPlayers();
    const firstAudio = AUDIOS[0];
    if (firstAudio && $(firstAudio)) {
      $(firstAudio).hidden = false;
    }
  }

  initOutputMap() {
    for (const audio of AUDIOS) {
      this.testOutputFeedback.set(audio, null);
    }
  }

  updateActiveOutputDevice() {
    const handler = AudioBroker.getInstance().handler;
    handler.getActiveOutputDeviceName().then(({deviceName}) => {
      if (deviceName) {
        $('active-output').innerHTML = deviceName;
      } else {
        $('active-output').innerHTML = 'No active output device';
      }
    });
  }

  setOutputMapEntry(audioFile: string, canHear: boolean) {
    this.testOutputFeedback.set(audioFile, canHear);
    console.log(this.testOutputFeedback);
  }

  createAudioPlayers() {
    for (var i = 0; i < AUDIOS.length; i++) {
      const current = AUDIOS[i];
      const prev = AUDIOS[i - 1];
      const next = AUDIOS[i + 1];
      if (current) {
        const audioPlayer = new AudioPlayer(current, prev, next);
        $('audio-players').appendChild(audioPlayer);
      }
    }
  }

  static getInstance() {
    if (instance === null) {
      instance = new OutputPage();
    }
    return instance;
  }
}

let instance: OutputPage|null = null;