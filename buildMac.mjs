// Script used by buildkite to build Chromium for macOS in CI
import { spawnChecked, updateRepo } from "./replay_build_scripts/common.mjs";

spawnChecked("node", ["build.js"], { stdio: "inherit" });
