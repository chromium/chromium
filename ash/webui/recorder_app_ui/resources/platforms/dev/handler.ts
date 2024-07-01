// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * TODO(pihsun): For simplicity sake currently everything is packed into
 * release. Reconsider this and whether we need the mock/alternative
 * implementation other than mojo to exist in release image.
 */
import 'chrome://resources/cros_components/dropdown/dropdown.js';
import 'chrome://resources/cros_components/dropdown/dropdown_option.js';

import {
  Dropdown,
} from 'chrome://resources/cros_components/dropdown/dropdown.js';
import {html, styleMap} from 'chrome://resources/mwc/lit/index.js';

import {SAMPLE_RATE} from '../../core/audio_constants.js';
import {
  Model,
  PlatformHandler as PlatformHandlerBase,
  SodaSession,
} from '../../core/platform_handler.js';
import {SodaEvent, TimeDelta} from '../../core/soda/types.js';
import {
  assert,
  assertEnumVariant,
  assertExists,
  assertInstanceof,
} from '../../core/utils/assert.js';
import {
  Observer,
  ObserverList,
  Unsubscribe,
} from '../../core/utils/observer_list.js';
import {sleep} from '../../core/utils/utils.js';

import {ColorTheme, devSettings, init as settingsInit} from './settings.js';
import {strings} from './strings.js';

class ModelDev implements Model {
  async suggestTitles(content: string): Promise<string[]> {
    await sleep(3000);
    const words = content.split(' ');
    return [
      `Title for "${words[2]}"`,
      `Longer long title for "${words[1]}"`,
      `This is a very long long title that is too long for "${words[0]}"`,
    ];
  }

  async summarize(content: string): Promise<string> {
    await sleep(3000);
    return `Summary for ${content.substring(0, 40)}...`;
  }

  close(): void {}
}

// Random placeholder text from ChromeOS blog.
const TRANSCRIPTION_LINES = `In July we announced that we were working on
Google Chrome OS, an open source operating system for people who spend most of
their time on the web.

Today we are open-sourcing the project as Chromium OS. We are doing this early,
a year before Google Chrome OS will be ready for users, because we are eager to
engage with partners, the open source community and developers. As with the
Google Chrome browser, development will be done in the open from this point on.

This means the code is free, accessible to anyone and open for contributions.
The Chromium OS project includes our current code base, user interface
experiments and some initial designs for ongoing development.

This is the initial sketch and we will color it in over the course of the next
year. We want to take this opportunity to explain why we're excited about the
project and how it is a fundamentally different model of computing.

First, it's all about the web. All apps are web apps. The entire experience
takes place within the browser and there are no conventional desktop
applications. This means users do not have to deal with installing, managing
and updating programs.

Second, because all apps live within the browser, there are significant
benefits to security. Unlike traditional operating systems, Chrome OS doesn't
trust the applications you run. Each app is contained within a security sandbox
making it harder for malware and viruses to infect your computer.

Furthermore, Chrome OS barely trusts itself. Every time you restart your
computer the operating system verifies the integrity of its code. If your
system has been compromised, it is designed to fix itself with a reboot.
While no computer can be made completely secure, we're going to make life
much harder (and less profitable) for the bad guys. If you dig security, read
the Chrome OS Security Overview or watch the video.

Most of all, we are obsessed with speed. We are taking out every unnecessary
process, optimizing many operations and running everything possible in
parallel. This means you can go from turning on the computer to surfing the web
in a few seconds. Our obsession with speed goes all the way down to the metal.

We are specifying reference hardware components to create the fastest
experience for Google Chrome OS.

There is still a lot of work to do, and we're excited to work with the open
source community. We have benefited hugely from projects like GNU, the Linux
Kernel, Moblin, Ubuntu, WebKit and many more. We will be contributing our code
upstream and engaging closely with these and other open source efforts.
Google Chrome OS will be ready for consumers this time next year. Sign up here
for updates or if you like building your operating system from source, get
involved at chromium.org.

Lastly, here is a short video that explains why we're so excited about Google
Chrome OS.`.split('\n\n').map((line) => line.split(/\s+/));

// Emit one word per 300 ms.
const WORD_INTERVAL_MS = 300;

function timeDelta(milliseconds: number): TimeDelta {
  return {microseconds: BigInt(milliseconds * 1000)};
}

class SodaSessionDev implements SodaSession {
  private readonly observers = new ObserverList<SodaEvent>();

  private numSamples = 0;

  private currentLineIdx = 0;

  private currentWordIdx = 0;

  private fakeTimeMs = 0;

  // TODO(pihsun): Simulate partial result being changed/corrected.
  private emitSodaNextWord(finishLine = false): void {
    this.fakeTimeMs += WORD_INTERVAL_MS;
    // Don't emit any word for the first two seconds, to simulate waiting state.
    if (this.fakeTimeMs <= 2000) {
      return;
    }
    const currentLine = assertExists(TRANSCRIPTION_LINES[this.currentLineIdx]);
    const timingEvent = {
      audioStartTime: timeDelta(
        this.fakeTimeMs - (this.currentWordIdx + 1) * WORD_INTERVAL_MS,
      ),
      eventEndTime: timeDelta(this.fakeTimeMs),
    };

    if (this.currentWordIdx === currentLine.length - 1 || finishLine) {
      const hypothesisPart =
        currentLine.slice(0, this.currentWordIdx + 1).map((w, i) => {
          return {
            text: [w],
            alignment: timeDelta(i * WORD_INTERVAL_MS),
            leadingSpace: true,
          };
        });
      this.observers.notify({
        finalResult: {
          finalHypotheses: currentLine,
          hypothesisPart,
          timingEvent,
        },
      });

      this.currentWordIdx = 0;
      this.currentLineIdx++;
      if (this.currentLineIdx === TRANSCRIPTION_LINES.length) {
        this.currentLineIdx = 0;
      }
    } else {
      this.observers.notify({
        partialResult: {
          partialText: [
            currentLine.slice(0, this.currentWordIdx + 1).join(' '),
          ],
          timingEvent,
        },
      });
      this.currentWordIdx++;
    }
  }

  async start(): Promise<void> {
    console.info('Soda session started');
  }

  addAudio(samples: Float32Array): void {
    console.debug(`Soda add audio of length ${samples.length}`);

    this.numSamples += samples.length;
    const wordIntervalSamples = (SAMPLE_RATE * WORD_INTERVAL_MS) / 1000;
    while (this.numSamples >= wordIntervalSamples) {
      this.emitSodaNextWord();
      this.numSamples -= wordIntervalSamples;
    }
  }

  async stop(): Promise<void> {
    console.info('Soda session stopped');
    this.emitSodaNextWord(true);
  }

  subscribeEvent(observer: Observer<SodaEvent>): Unsubscribe {
    return this.observers.subscribe(observer);
  }
}

/**
 * Returns a formatted localized string where $1 to $9 are replaced by the
 * second to the tenth argument. Any standalone $ signs must be escaped as
 * $$.
 *
 * This is a copy of the function in
 * ui/webui/resources/js/load_time_data.ts to avoid pulling those as
 * dependency for dev.
 */
function substituteI18nString(label: string, ...args: Array<number|string>):
  string {
  return label.replace(/\$(.|$|\n)/g, (m) => {
    assert(
      m.match(/\$[$1-9]/) !== null,
      'Unescaped $ found in localized string.',
    );
    if (m === '$$') {
      return '$';
    }

    const substitute = args[Number(m[1]) - 1];
    if (substitute === undefined || substitute === null) {
      // Not all callers actually provide values for all substitutes. Return
      // an empty value for this case.
      return '';
    }
    return substitute.toString();
  });
}

export class PlatformHandler extends PlatformHandlerBase {
  override init(): void {
    settingsInit();
  }

  override async loadModel(_uuid: string): Promise<Model> {
    return new ModelDev();
  }

  override async newSodaSession(): Promise<SodaSession> {
    return new SodaSessionDev();
  }

  override getStringF(id: string, ...args: Array<number|string>): string {
    const label = strings[id];
    if (label === undefined) {
      console.error(`Unknown string ${id}`);
      return '';
    }
    return substituteI18nString(label, ...args);
  }

  override renderDevUi(): RenderResult {
    function handleChange(ev: Event) {
      devSettings.mutate((s) => {
        s.forceTheme = assertEnumVariant(
          ColorTheme,
          assertInstanceof(ev.target, Dropdown).value,
        );
      });
    }
    // TODO(pihsun): Move the dev toggle to a separate component, so we don't
    // need to inline the styles.
    const labelStyle = {
      display: 'flex',
      flexFlow: 'row',
      alignItems: 'center',
    };
    return html`
      <div class="section">
        <label style=${styleMap(labelStyle)}>
          <cros-dropdown
            label="dark/light mode"
            @change=${handleChange}
            .value=${devSettings.value.forceTheme ?? ColorTheme.SYSTEM}
          >
            <cros-dropdown-option
              headline="System default"
              value=${ColorTheme.SYSTEM}
            >
            </cros-dropdown-option>
            <cros-dropdown-option headline="Light" value=${ColorTheme.LIGHT}>
            </cros-dropdown-option>
            <cros-dropdown-option headline="Dark" value=${ColorTheme.DARK}>
            </cros-dropdown-option>
          </cros-dropdown>
        </label>
      </div>
    `;
  }
}
