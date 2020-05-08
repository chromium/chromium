const qrvr = require('qrvr');

const port = parseInt(process.argv[2], 10);

// console.log('got args', port);

qrvr.start({
  port,
}).then(() => {
  console.log('qr reader started');
});