const fs = require("fs");
const os = require("os");
const { spawnSync } = require("child_process");

// Generate a new build ID.
const buildId = `${currentPlatform()}-chromium-${makeDate()}-${makeRandomId()}`;

fs.writeFileSync(
  `${__dirname}/base/record_replay_build_id.cc`,
  `namespace recordreplay { char gBuildId[] = "${buildId}"; }`
);

if (currentPlatform() == "macOS") {
  // Make sure the main executable gets rebuilt with the new build ID.
  spawnChecked("touch", [`${__dirname}/chrome/app/chrome_exe_main_mac.cc`]);
}

spawnChecked("autoninja", ["-C", "out/Release", "chrome"], { stdio: "inherit" });

function spawnChecked(cmd, args, options) {
  const prettyCmd = [cmd].concat(args).join(" ");
  console.error(prettyCmd);

  const rv = spawnSync(cmd, args, options);

  if (rv.status != 0 || rv.error) {
    console.error(rv.error);
    throw new Error(`Spawned process failed with exit code ${rv.status}`);
  }

  return rv;
}

function currentPlatform() {
  switch (process.platform) {
    case "darwin":
      return "macOS";
    case "linux":
      return "linux";
    default:
      throw new Error(`Platform ${process.platform} not supported`);
  }
}

function makeDate() {
  const now = new Date();
  const year = now.getFullYear();
  const month = (now.getMonth() + 1).toString().padStart(2, "0");
  const date = now.getDate().toString().padStart(2, "0");
  return `${year}${month}${date}`;
}

function makeRandomId() {
  return Math.round(Math.random() * 1e9).toString();
}
