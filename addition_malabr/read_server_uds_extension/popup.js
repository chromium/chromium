document.getElementById('readDataBtn').addEventListener('click', () => {
  chrome.readServerUds.readData((response) => {
    try {
      const parsedResponse = JSON.parse(response);

      // if (parsedResponse.status) {
        console.log('Server Response:', parsedResponse);
        alert('Server Response: ' + JSON.stringify(parsedResponse));
      // } else {
      //   console.error('Error:', parsedResponse);
      //   alert('Error: ' + JSON.stringify(parsedResponse));
      // }
    } catch (e) {
      console.error('Failed to parse response:', e);
      alert('Error: Failed to parse response');
    }
  });
});
